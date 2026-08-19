#include "process.h"
#include "kernel.h"
#include "util/spinlock.h"
#include <mm/heap.h>
#include <memory.h>
#include <printfwrapper.h>
#include "sched.h"
#include <mm/pmm.h>
#include <ksyscall.h>
#include <util/util.h>
#include <kedriver.h>
#include <sched/ipc/signal.h>
#include <external/posix/signal.h>
#include <sched/ipc/signaltrampoline.h>
#ifdef __x86_64__
#include <arch/x86_64/hal.h>
#endif
extern Spinlock SchedSpinlock;
extern ThreadCtrlBlk* CurrentThread;
extern ThreadCtrlBlk* ReadyQueueHead;
extern ThreadCtrlBlk* DeathThread;

static uint64_t PidCount = 0;

static uint64_t ProcGetPid() {
    uint64_t r = PidCount++;
    return r;
}

uint64_t* ProcNewPML4() {
    uint64_t* NewPML4Phys = PmmAllocate();
    uint64_t* NewPML4 = (uint64_t*)((uint64_t)NewPML4Phys + gMmuVOffset);
    memset(NewPML4, 0, PAGE_SIZE);
    uint64_t* KernelPML4 = (uint64_t*)((uint64_t)_x86_64_get_pml4() + gMmuVOffset);
    memcpy(&NewPML4[256], &KernelPML4[256], 256 * sizeof(uint64_t));
    return NewPML4Phys;
}

// frees everything except pml4p itself
void ProcFreeInnerPML4(pagetable* pml4p) {
    pagetable* pml4 = (pagetable*)P2V(pml4p);
    for (int i = 0; i < 256; i++) {
        if (pml4[i] & MMU_PAGE_BIT_P_PRESENT) {
            pagetable* pdpt = (pagetable*)(P2V(pml4[i] & ~0xFFF));
            for (int j = 0; j < 512; j++) {
                if (pdpt[j] & MMU_PAGE_BIT_P_PRESENT) {
                    pagetable* pd = (pagetable*)(P2V(pdpt[j] & ~0xFFF));
                    for (int k = 0; k < 512; k++) {
                        if (pd[k] & MMU_PAGE_BIT_P_PRESENT) {
                            physaddr pt_phys = pd[k] & ~0xFFF;
                            pd[k] = 0x0;
                            PmmFree((void*)pt_phys);
                        }
                    }
                    pdpt[j] = 0x0;
                    PmmFree((void*)(pdpt[j] & ~0xFFF));
                }
            }
            pml4[i] = 0x0;
            PmmFree((void*)(pml4[i] & ~0xFFF));
        }
    }
}
void ProcFreePML4(pagetable* pml4p) {
    ProcFreeInnerPML4(pml4p);
    PmmFree((void*)pml4p);
}

ProcessCtrlBlk* ProcFindByPid(uint64_t pid, KernelInformation* kinfo) {
    ProcessCtrlBlk* list = kinfo->ProcessListHead;
    if (!list) return NULL;
    ProcessCtrlBlk* current = list;
    while (current != NULL) {
        if (current->pid == pid) {
            return current;
        }
        current = current->Next;
    }
    return NULL;
}

void ProcListRunning(KernelInformation* kinfo) {
    ProcessCtrlBlk* list = kinfo->ProcessListHead;
    if (!list) return;
    ProcessCtrlBlk* current = list;
    printf("process: List of running processes: \r\n");
    while (current != NULL) {
        printf("process:     [*] %s (pid=%d)\r\n", current->name, current->pid);
        ThreadCtrlBlk* thrc = current->ThreadListHead;
        while (thrc != NULL) {
            printf("process:            [*] TID %d\r\n", thrc->tid);
            thrc = thrc->ProcNext;
        }
        current = current->Next;
    }
}
void ThreadCreateKrnlStack(ThreadCtrlBlk* Tcb, void* entry) {
    uint64_t* StackBase = (uint64_t*)MmAllocate(16384);
    uint64_t* StackTop = StackBase + (16384 / 8);
    //Tcb->StackBase = (uint64_t)StackTop - (16384 * 8);
    if ((uint64_t)StackTop % 16 != 0) StackTop--;
    *(--StackTop) = (uint64_t)ThreadEntry;

    for (int i = 0; i < 6; i++) {
        *(--StackTop) = 0;
    }
    *(--StackTop) = 0x202;
    Tcb->KernelRsp = (uint64_t)StackTop;
    Tcb->KernelStackBase = (uint64_t)StackBase;
    Tcb->entry = entry;
}
void ThreadCreateUserStack(ThreadCtrlBlk* Tcb, void* entry, const char** argv, int argc, const char** envp, int envc) {
    uint64_t StackSize = PS_USER_STACK_PAGES * PAGE_SIZE;
    uint64_t* StackBasePhys = (uint64_t*)PmmAllocatePages(PS_USER_STACK_PAGES);

    Tcb->UserStackBase = (uint64_t)P2V(StackBasePhys);

    uint64_t UserStackTop = PS_USER_STACK_BASE;
    uint64_t LocalVirt  = UserStackTop;
    uint64_t KernelVirt = Tcb->UserStackBase + StackSize;

    uint64_t* UserArgvAddresses = MmAllocate(sizeof(uint64_t) * argc);
    uint64_t* UserEnvpAddresses = MmAllocate(sizeof(uint64_t) * envc);

    // argv
    for (int i = argc - 1; i >= 0; i--) {
        uint64_t len = strlen(argv[i]) + 1;
        LocalVirt  -= len;
        KernelVirt -= len;
        memcpy((void*)KernelVirt, argv[i], len);
        UserArgvAddresses[i] = LocalVirt;
    }

    // envp
    for (int i = envc - 1; i >= 0; i--) {
        uint64_t len = strlen(envp[i]) + 1;
        LocalVirt  -= len;
        KernelVirt -= len;
        memcpy((void*)KernelVirt, envp[i], len);
        UserEnvpAddresses[i] = LocalVirt;
    }
    // todo: ?
    uint64_t align8 = LocalVirt % 8;
    LocalVirt  -= align8;
    KernelVirt -= align8;

    uint64_t block_words = 1 + (argc + 1) + (envc + 1) + (2 * 2);
    uint64_t block_bytes = block_words * sizeof(uint64_t);

    uint64_t block_start = LocalVirt - block_bytes;
    block_start &= ~0xFULL;

    uint64_t delta = LocalVirt - block_start;
    LocalVirt  -= delta;
    KernelVirt -= delta;

    uint64_t* kw = (uint64_t*)KernelVirt;
    int idx = 0;

    kw[idx++] = (uint64_t)argc;
    // fix here
    for (int i = 0; i < argc; i++)
        kw[idx++] = UserArgvAddresses[i];
    kw[idx++] = 0;

    for (int i = 0; i < envc; i++)
        kw[idx++] = UserEnvpAddresses[i];
    kw[idx++] = 0;

    kw[idx++] = 6;
    kw[idx++] = PAGE_SIZE; // depends on 2mb pages but we dont use those yet
    kw[idx++] = 0;
    kw[idx++] = 0;

    MmFree(UserArgvAddresses);
    MmFree(UserEnvpAddresses);

    Tcb->UserRsp  = LocalVirt;
    Tcb->UserArgc = argc;
}

void ThreadMapUserStack(ThreadCtrlBlk* Tcb) {
    if (!Tcb->ParentProc) return; // no parent proc page tables to map
    if (!Tcb->UserStackBase) return;
    uint64_t StackBasePhys = V2P(Tcb->UserStackBase);
    uint64_t StackSize = PS_USER_STACK_PAGES * PAGE_SIZE;
    uint64_t PhysLimit = StackBasePhys + StackSize;
    uint64_t StartVirt =  PS_USER_STACK_BASE - (StackSize);
    uint64_t CurrentVirt = StartVirt;
    for (uint64_t phys = StackBasePhys; phys < PhysLimit; phys += PAGE_SIZE) {
        MmuMapPage((pagetable*)P2V(Tcb->ParentProc->cr3), CurrentVirt, phys, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_US_USER);
        CurrentVirt += PAGE_SIZE;
    }
    // also map signal trampoline
    void* SignalTrampoline = PmmAllocate();
    memset((void*)P2V(SignalTrampoline), 0, MMU_PAGE_SIZE);
    memcpy((void*)P2V(SignalTrampoline), __signal_trampoline, sizeof(__signal_trampoline));
    MmuMapPage((pagetable*)P2V(Tcb->ParentProc->cr3), KE_LDR_SIGNAL_ADDR, (physaddr)SignalTrampoline, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_US_USER);
    // for (int i = 0; i < 10; i++) printf("thrmap: SignalTrampoline(mapped into proc cr3)[%d]=%x\r\n", i, vtmp[i]);
}

ThreadCtrlBlk* ThreadNew(void* entry, uint8_t priv, const char** argv, int argc, const char** envp, int envc) {
    ThreadCtrlBlk* new = MmAllocate(sizeof(ThreadCtrlBlk));
    memset(new, 0, sizeof(ThreadCtrlBlk));
    new->state = SCHED_THREAD_READY;
    //new->tid = ThreadGetTid();
    new->privilege = priv;
    ThreadCreateKrnlStack(new, entry);
    if (priv > SCHED_PRIV_KERNEL) {
        ThreadCreateUserStack(new, entry, argv, argc, envp, envc);
    }
    new->exitcode = 0;
    new->FsBase = 0;
    new->GsBase = 0;
    return new;
}

// creates a new process and it makes one thread with the entry point
ProcessCtrlBlk* ProcessNew(char* name) {
    ProcessCtrlBlk* new = MmAllocate(sizeof(ProcessCtrlBlk));
    memcpy((void*)new->name, (void*)name, strlen(name)+1);
    memcpy((void*)new->cwd, (const void*)"initrd:/programs", 17);
    new->pml4 = ProcNewPML4();
    new->cr3 = (uint64_t)new->pml4;
    new->pid = ProcGetPid()+1;
    new->nextfh = 3; // reserve '0' for stdout '1' for stderr '2' for stdin
    new->threads = 0;
    new->FileHandleTable = MmAllocate(sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    new->SbrkBase = PS_USER_BRK_BASE;
    new->SbrkCurrent = PS_USER_BRK_BASE;
    new->SbrkLimit = PS_USER_BRK_BASE + PS_USER_BRK_SIZE;
    new->Parent = KernelGetInformation()->KernelProcess;
    new->Next = NULL;
    new->exitcode = 0;
    new->MmapEntryHead = NULL;
    new->MmapBumpNext = PS_USER_MMAPDEC_BASE;
    new->BlockedQueueHead = NULL;
    new->BlockedQueueTail = NULL;
    new->FileHandleTable[VFS_HANDLE_STDIN].Flag = VFS_OFD_FLAG_CNSL;
    new->FileHandleTable[VFS_HANDLE_STDOUT].Flag = VFS_OFD_FLAG_CNSL;
    new->FileHandleTable[VFS_HANDLE_STDERR].Flag = VFS_OFD_FLAG_CNSL;
    new->TtyObj = TtyCreateObj(VFS_HANDLE_STDIN, VFS_HANDLE_STDOUT, VFS_HANDLE_STDERR);
    KATTEMPT(KeSignalInitDef(new) == KSUCCESS);
    return new;
}

ThreadCtrlBlk* ThrGetCurrent() {
    uint64_t r = SpnLckAcquireRfl(&SchedSpinlock);
    ThreadCtrlBlk* c = CurrentThread;
    SpnLckReleaseRfl(&SchedSpinlock, r);
    return c;
}
KE_EXPORT_SYMBOL(ThrGetCurrent);
// checks if a signal is pending
// sm bs always randomly jumping to this static Spinlock SignalChkLock = {ATOMIC_FLAG_INIT};

void ThrCheckSignals(CpuInterruptArgs* OldCtx) {
    if (!OldCtx) return;
    // uint64_t r = SpnLckAcquireRfl(&SignalChkLock);
    uint64_t Deliver = CurrentThread->SigPendingSet & ~CurrentThread->SigBlockedSet;
    if (!Deliver) return;
    uint8_t SigIdx = __builtin_ctzll(Deliver);
    CurrentThread->SigPendingSet &= ~(1ULL << SigIdx);
    if (SigIdx == SIGKILL) {
        KE_SYSCALL_CALL_ARG1(SysExit, (uint64_t)-1);
        return;
    }
    KeSignalHdlObj* SigObj = &CurrentThread->ParentProc->Handlers[SigIdx];

    if (SigObj->Handler == KE_SIGLIST_ADDR_DEFAULTIGN) {
        // ignore it
        return;
    }

    if (SigObj->Handler == KE_SIGLIST_ADDR_DEFAULTKHDL) {
        switch (KeSignalDefAct(SigIdx)) {
            case KE_SIGNAL_DEF_TERMINATE:
            case KE_SIGNAL_DEF_COREDUMP:
                KE_SYSCALL_CALL_ARG1(SysExit, (uint64_t)512 + SigIdx);
                return;
            case KE_SIGNAL_DEF_IGNORE:
                return;
            case KE_SIGNAL_DEF_STOP:
                // todo cuz no job control
                return;
            case KE_SIGNAL_DEF_CONT:
                // same
                return;
        }
    }
    // SpnLckReleaseRfl(&SignalChkLock, r);
    KeSignalHandle(CurrentThread, SigIdx, SigObj, OldCtx);
}

void ProcessCreate(void* entry, KernelInformation* kinfo, uint8_t priv) {
    ProcessCtrlBlk* proc = ProcessNew("noname");
    ThreadCtrlBlk* thr = ThreadNew(entry, priv, 0, 0, 0, 0);
    ProcAttachThread(proc, thr);
    if (priv > SCHED_PRIV_KERNEL) {
        ThreadMapUserStack(thr);
    }
    ThreadAdd(thr);
    proc->Next = kinfo->ProcessListHead;
    kinfo->ProcessListHead = proc;
    kinfo->CurrentProcess = proc;
}
extern void isr_syscall_resume();
// creates a copy of a process
// only difference being actual pid and page table addr
// its same thing as fork
uint64_t ProcessCopy(ProcessCtrlBlk* proc, ThreadCtrlBlk* caller, CpuInterruptArgs* frame) {
    ProcessCtrlBlk* new = ProcessNew(proc->name);
    if (!new) {
        return (uint64_t)-1;
    }

    int copyResult = MmuForkCopyUserSpace((pagetable*)proc->cr3, (pagetable*)new->cr3);
    if (copyResult != 0) {
        return (uint64_t)-1;
    }
    new->nextfh = proc->nextfh;
    new->SbrkBase = proc->SbrkBase;
    new->SbrkCurrent = proc->SbrkCurrent;
    new->SbrkLimit = proc->SbrkLimit;
    new->Parent = proc;
    new->MmapEntryHead = proc->MmapEntryHead; // lazy
    new->MmapBumpNext = proc->MmapBumpNext;
    memcpy((void*)new->cwd, (void*)proc->cwd, strlen(proc->cwd)+1);
    memcpy((void*)new->FileHandleTable, proc->FileHandleTable, sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    ThreadCtrlBlk* thr = MmAllocate(sizeof(ThreadCtrlBlk));
    memset(thr, 0, sizeof(ThreadCtrlBlk));
    thr->state = SCHED_THREAD_READY;
    thr->privilege = caller->privilege;
    thr->exitcode = 0;

    // fake a stack cuz if we use cpuinterruptargs directly itll pop absolute garbage
    uint64_t* StackBase = (uint64_t*)MmAllocate(16384);
    uint64_t* StackTop = StackBase + (16384 / 8);

    StackTop = (uint64_t*)((uint64_t)StackTop - sizeof(CpuInterruptArgs));
    CpuInterruptArgs* ChildFrame = (CpuInterruptArgs*)StackTop;

    memcpy(ChildFrame, frame, sizeof(CpuInterruptArgs));
    ChildFrame->rax = 0;

    if ((uint64_t)StackTop % 16 != 0) StackTop--;

    *(--StackTop) = (uint64_t)isr_syscall_resume;
    *(--StackTop) = (uint64_t)ChildFrame;
    *(--StackTop) = 0;
    *(--StackTop) = 0;
    *(--StackTop) = 0;
    *(--StackTop) = 0;
    *(--StackTop) = 0;
    *(--StackTop) = 0x202;

    thr->KernelRsp = (uint64_t)StackTop;
    thr->KernelStackBase = (uint64_t)StackBase;
    uint64_t StackBaseVirt = PS_USER_STACK_BASE - (PS_USER_STACK_PAGES * MMU_PAGE_SIZE);
    uint64_t ChildStackBasePhys = MmuGetPhys(new->cr3, StackBaseVirt);
    if (!ChildStackBasePhys) {
        // shouldnt really happen
        return (uint64_t)-1;
    }
    thr->UserStackBase = (uint64_t)P2V(ChildStackBasePhys);
    thr->UserRsp = caller->UserRsp;
    ProcAttachThread(new, thr);
    new->Next = KernelGetInformation()->ProcessListHead;
    KernelGetInformation()->ProcessListHead = new;
    ThreadAdd(thr);
    return new->pid;
}


void ThreadAdd(ThreadCtrlBlk* Tcb) {
    Tcb->GlobalNext = ReadyQueueHead;
    ReadyQueueHead = Tcb;
}

void ThreadWake(ThreadCtrlBlk* Tcb) {
    if (!Tcb) return;

    uint64_t r = SpnLckAcquireRfl(&SchedSpinlock);

    Tcb->state = SCHED_THREAD_READY;
    Tcb->GlobalNext = NULL;

    if (ReadyQueueHead == NULL) {
        ReadyQueueHead = Tcb;
    } else {
        ThreadCtrlBlk* current = ReadyQueueHead;
        while (current->GlobalNext != NULL) {
            current = current->GlobalNext;
        }
        current->GlobalNext = Tcb;
    }

    SpnLckReleaseRfl(&SchedSpinlock, r);
}
KE_EXPORT_SYMBOL(ThreadWake);

void ThreadPushTail(ThreadCtrlBlk** Head, ThreadCtrlBlk** Tail, ThreadCtrlBlk* Tcb) {
    if (!Tcb) return;

    Tcb->GlobalNext = NULL;

    if (*Tail == NULL) {
        *Head = Tcb;
        *Tail = Tcb;
    } else {
        (*Tail)->GlobalNext = Tcb;
        *Tail = Tcb;
    }
}
KE_EXPORT_SYMBOL(ThreadPushTail);

ThreadCtrlBlk* ThreadPopHead(ThreadCtrlBlk** Head, ThreadCtrlBlk** Tail) {
    if (Head == NULL || *Head == NULL) return NULL;

    ThreadCtrlBlk* Tcb = *Head;
    *Head = Tcb->GlobalNext;

    if (*Head == NULL) {
        *Tail = NULL;
    }

    Tcb->GlobalNext = NULL;
    return Tcb;
}
KE_EXPORT_SYMBOL(ThreadPopHead);

void ThreadEntry() {
    // SpnLckRelease(&SchedSpinlock);
    if (DeathThread != NULL) {
        if (DeathThread->KernelStackBase) {
            MmFree((void*)DeathThread->KernelStackBase);
        }
        if(DeathThread->ParentProc->threads <= 0) {
            // remove from kernel list
            ProcessCtrlBlk* ProcList = KernelGetInformation()->ProcessListHead;
            
            if (DeathThread->ParentProc == ProcList){ ProcList = ProcList->Next;} else {
                ProcessCtrlBlk* current = ProcList;
                ProcessCtrlBlk* previous;
                while (current != NULL) {
                    if (current == DeathThread->ParentProc) break;
                    previous = current;
                    current = current->Next;
                }
                KATTEMPT(current);
                if (!(current == DeathThread->ParentProc)) goto _s;
                KATTEMPT(previous);
                previous->Next = current->Next;
            }
            _s:
            KernelUnlockRsLck();
            MmFree(DeathThread->ParentProc->FileHandleTable);
            ProcFreePML4(DeathThread->ParentProc->pml4);
            if (DeathThread->ParentProc->TtyObj) MmFree(DeathThread->ParentProc->TtyObj);
            if (DeathThread->ParentProc->MmapEntryHead) {
                MmapEntry* c = DeathThread->ParentProc->MmapEntryHead;
                MmapEntry* n;
                while (c != NULL) {
                    n = c->Next;
                    MmFree(c);
                    c = n;
                }
            }
            MmFree(DeathThread->ParentProc);
        }
        MmFree(DeathThread);
        DeathThread = NULL; 
    }
    asm volatile ("sti");
    //printf("sched: wrapper: entering thread(tid=%d, entry=0x%lx)\r\n", CurrentThread->tid, CurrentThread->entry);
    void (*entry)() = CurrentThread->entry;
    if (entry) {
        switch (CurrentThread->privilege) {
            case SCHED_PRIV_KERNEL: {
                entry();
                break;
            }
            case SCHED_PRIV_USER: {
                asm volatile ("cli");
                HalUserJump((uint64_t)entry, CurrentThread->UserRsp, (uint64_t)CurrentThread->UserArgv, (uint64_t)CurrentThread->UserArgc);
                break;
            }
        }
    }
    KE_SYSCALL_CALL_ARG1(SysExit, 0);
    KSUCCESS(KFAIL);
}

void ProcAttachThread(ProcessCtrlBlk* proc, ThreadCtrlBlk* tcb) {
    tcb->ParentProc = proc;
    tcb->ProcNext = proc->ThreadListHead;
    proc->ThreadListHead = tcb;
    proc->threads++;
    tcb->tid = proc->threads-1;
}