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

void ProcFreePML4(pagetable* pml4p) {
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
                            PmmFree((void*)pt_phys);
                        }
                    }
                    PmmFree((void*)(pdpt[j] & ~0xFFF));
                }
            }
            PmmFree((void*)(pml4[i] & ~0xFFF));
        }
    }
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
void ThreadCreateUserStack(ThreadCtrlBlk* Tcb, void* entry, const char** argv, int argc) {
    uint64_t StackSize = PS_USER_STACK_PAGES * PAGE_SIZE;
    uint64_t* StackBasePhys = (uint64_t*)PmmAllocatePages(PS_USER_STACK_PAGES);

    Tcb->UserStackBase = (uint64_t)P2V(StackBasePhys);

    uint64_t UserStackTop = PS_USER_STACK_BASE;
    uint64_t LocalVirt  = UserStackTop;
    uint64_t KernelVirt = Tcb->UserStackBase + StackSize;

    uint64_t* UserArgvAddresses = MmAllocate(sizeof(uint64_t) * argc);

    for (int i = argc - 1; i >= 0; i--) {
        uint64_t len = strlen(argv[i]) + 1;
        LocalVirt  -= len;
        KernelVirt -= len;
        memcpy((void*)KernelVirt, argv[i], len);
        UserArgvAddresses[i] = LocalVirt;
    }

    uint64_t align8 = LocalVirt % 8;
    LocalVirt  -= align8;
    KernelVirt -= align8;

    uint64_t block_words = 1 + (argc + 1) + 1 + (2 * 2);
    uint64_t block_bytes = block_words * sizeof(uint64_t);

    uint64_t block_start = LocalVirt - block_bytes;
    block_start &= ~0xFULL;

    uint64_t delta = LocalVirt - block_start;
    LocalVirt  -= delta;
    KernelVirt -= delta;

    uint64_t* kw = (uint64_t*)KernelVirt;
    int idx = 0;

    kw[idx++] = (uint64_t)argc;

    for (int i = 0; i < argc; i++)
        kw[idx++] = UserArgvAddresses[i];

    kw[idx++] = 0;
    kw[idx++] = 0;

    kw[idx++] = 6;
    kw[idx++] = PAGE_SIZE;
    kw[idx++] = 0;
    kw[idx++] = 0;

    MmFree(UserArgvAddresses);

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
}

ThreadCtrlBlk* ThreadNew(void* entry, uint8_t priv, const char** argv, int argc) {
    ThreadCtrlBlk* new = MmAllocate(sizeof(ThreadCtrlBlk));
    memset(new, 0, sizeof(ThreadCtrlBlk));
    new->state = SCHED_THREAD_READY;
    //new->tid = ThreadGetTid();
    new->privilege = priv;
    ThreadCreateKrnlStack(new, entry);
    if (priv > SCHED_PRIV_KERNEL) {
        ThreadCreateUserStack(new, entry, argv, argc);
    }
    new->exitcode = 0;
    new->pendingkill = 0;
    return new;
}

// creates a new process and it makes one thread with the entry point
ProcessCtrlBlk* ProcessNew(char* name) {
    ProcessCtrlBlk* new = MmAllocate(sizeof(ProcessCtrlBlk));
    memcpy((void*)new->name, (void*)name, strlen(name)+1);
    new->pml4 = ProcNewPML4();
    new->cr3 = (uint64_t)new->pml4;
    new->pid = ProcGetPid()+1;
    new->nextfh = 3; // reserve '0' for stdout '1' for stderr '2' for stdin
    new->threads = 0;
    new->FileHandleTable = MmAllocate(sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    new->SbrkBase = PS_USER_BRK_BASE;
    new->SbrkCurrent = PS_USER_BRK_BASE;
    new->SbrkLimit = PS_USER_BRK_BASE + PS_USER_BRK_SIZE;
    new->Parent = NULL;
    new->Next = NULL;
    return new;
}

ThreadCtrlBlk* ThrGetCurrent() {
    SpnLckAcquire(&SchedSpinlock);
    ThreadCtrlBlk* c = CurrentThread;
    SpnLckRelease(&SchedSpinlock);
    return c;
}
KE_EXPORT_SYMBOL(ThrGetCurrent);
void ThrCheckPendingKill() {
    if (CurrentThread->pendingkill) {
        KE_SYSCALL_CALL_ARG1(SysExit, -1);
    }
}

void ProcessCreate(void* entry, KernelInformation* kinfo, uint8_t priv) {
    ProcessCtrlBlk* proc = ProcessNew("noname");
    ThreadCtrlBlk* thr = ThreadNew(entry, priv, 0, 0);
    ProcAttachThread(proc, thr);
    if (priv > SCHED_PRIV_KERNEL) {
        ThreadMapUserStack(thr);
    }
    ThreadAdd(thr);
    proc->Next = kinfo->ProcessListHead;
    kinfo->ProcessListHead = proc;
    kinfo->CurrentProcess = proc;
}

void ThreadAdd(ThreadCtrlBlk* Tcb) {
    Tcb->GlobalNext = ReadyQueueHead;
    ReadyQueueHead = Tcb;
}

void ThreadWake(ThreadCtrlBlk* Tcb) {
    if (!Tcb) return;

    SpnLckAcquire(&SchedSpinlock);

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

    SpnLckRelease(&SchedSpinlock);
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
    SpnLckRelease(&SchedSpinlock);
    if (DeathThread != NULL) {
        if (DeathThread->KernelStackBase) {
            MmFree((void*)DeathThread->KernelStackBase);
        }
        if (DeathThread->UserStackBase) {
            PmmFreePages((void*)V2P(DeathThread->UserStackBase), PS_USER_STACK_PAGES);
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
                printf("process: execute process entry @ 0x%lx\r\n", entry);
                entry();
                break;
            }
            case SCHED_PRIV_USER: {
                printf("process: execute process entry @ 0x%lx\r\n", entry);
                printf("process: argv=0x%lx argc=0x%lx in user mode.\r\n", CurrentThread->UserArgv, CurrentThread->UserArgc);
                asm volatile ("cli");
                _x86_64_usjmp((uint64_t)entry, CurrentThread->UserRsp, (uint64_t)CurrentThread->UserArgv, (uint64_t)CurrentThread->UserArgc);
                break;
            }
        }
    }
    KE_SYSCALL_CALL_ARG1(SysExit, 0);
    while (1) {asm("hlt");}
}

void ProcAttachThread(ProcessCtrlBlk* proc, ThreadCtrlBlk* tcb) {
    tcb->ParentProc = proc;
    tcb->ProcNext = proc->ThreadListHead;
    proc->ThreadListHead = tcb;
    proc->threads++;
    tcb->tid = proc->threads-1;
}