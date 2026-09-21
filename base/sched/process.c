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
#include <hal/mmu.h>
#include <hal/ps.h>

extern Spinlock SchedSpinlock;
extern ThreadCtrlBlk* CurrentThread;
extern ThreadCtrlBlk* ReadyQueueHead;
extern ThreadCtrlBlk* DeathThread;

static uint64_t PidCount = 0;

static uint64_t ProcGetPid() {
    uint64_t r = PidCount++;
    return r;
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
    #ifdef __x86_64__
    new->FsBase = 0;
    new->GsBase = 0;
    #endif
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
    new->MessageHead = NULL;
    new->MessageTail = NULL;
    new->MessageCount = 0;
    new->ThreadListHead = NULL;
    memset(&new->MessageQueueLock, 0, sizeof(Spinlock));
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
    new->MmapEntryHead = NULL;
    new->MmapBumpNext = PS_USER_MMAPDEC_BASE;
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

void ThrDeathCleanup() {
    if (DeathThread != NULL) {
        uint64_t r = SpnLckAcquireRfl(&SchedSpinlock);
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
            MmFree(DeathThread->ParentProc);
        }
        MmFree(DeathThread);
        DeathThread = NULL;
        SpnLckReleaseRfl(&SchedSpinlock, r);
    }
}
void ThreadEntry() {
    // SpnLckRelease(&SchedSpinlock);
    ThrDeathCleanup();
    HAL_INT_ON();
    //printf("sched: wrapper: entering thread(tid=%d, entry=0x%lx)\r\n", CurrentThread->tid, CurrentThread->entry);
    void (*entry)() = CurrentThread->entry;
    if (entry) {
        switch (CurrentThread->privilege) {
            case SCHED_PRIV_KERNEL: {
                entry();
                break;
            }
            case SCHED_PRIV_USER: {
                HAL_INT_OFF();
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