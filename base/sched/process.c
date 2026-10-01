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
extern KeSchedQueue DeathQueue;
extern ThreadCtrlBlk* IdleThreadPtr;

static uint64_t PidCount = 0;

static uint64_t ProcGetPid() {
    uint64_t r = PidCount++;
    return r;
}
static const uint32_t TicksDefault[SCHED_THREAD_PHIGH+1] = {
    64, // SCHED_THREAD_PLOW
    63,
    62,
    61,
    60,
    59,
    58,
    57,
    56,
    55,
    54,
    53,
    52,
    51,
    50,
    49,
    48,
    32, // SCHED_THREAD_PMEDIUM
    31,
    30,
    29,
    28,
    27,
    26,
    25,
    24,
    23,
    22,
    21,
    20,
    10,
    9,
    8,
    4, // SCHED_THREAD_PHIGH
};

static uint32_t ThrDecideTicksDefault(uint32_t Priority) {
    return TicksDefault[Priority];
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
        ThreadCtrlBlk* thrc = current->ThreadList->Head;
        while (thrc != NULL) {
            printf("process:            [*] TID %d (prior=%d state=%d)\r\n", thrc->tid, thrc->Priority, thrc->State);
            thrc = thrc->ProcNext;
        }
        current = current->Next;
    }
}

ThreadCtrlBlk* ThreadNew(void* entry, uint8_t priv, uint32_t prior, const char** argv, int argc, const char** envp, int envc) {
    ThreadCtrlBlk* new = MmAllocate(sizeof(ThreadCtrlBlk));
    memset(new, 0, sizeof(ThreadCtrlBlk));
    new->State = SCHED_THREAD_READY;
    //new->tid = ThreadGetTid();
    new->Priority = prior;
    new->Bpriority = prior;
    new->tickdefault = ThrDecideTicksDefault(prior);
    new->deadline = new->tickdefault;
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
    uint64_t NameSz = strlen(name);
    if (NameSz >= sizeof(new->name)) NameSz = sizeof(new->name) - 1;
    memcpy(new->name, name, NameSz);
    new->name[NameSz] = '\0';
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
    new->ThreadList = MmAllocate(sizeof(KeSchedQueue));
    new->ThreadList->Head = NULL;
    new->ThreadList->Tail = NULL;
    memset(&new->MessageQueueLock, 0, sizeof(Spinlock));
    memset(&new->MmapListLock, 0, sizeof(Spinlock));
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

    if ((uint64_t)SigObj->Handler == KE_SIGLIST_ADDR_DEFAULTIGN) {
        // ignore it
        return;
    }

    if ((uint64_t)SigObj->Handler == KE_SIGLIST_ADDR_DEFAULTKHDL) {
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
    ThreadCtrlBlk* thr = ThreadNew(entry, priv, SCHED_THREAD_PLOW, 0, 0, 0, 0);
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
    thr->State = SCHED_THREAD_READY;
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



void ThreadQueueAdd(struct KeSchedQueue* Queue, ThreadCtrlBlk* Tcb) {
    Tcb->GlobalNext = NULL;
    Tcb->GlobalPrev = Queue->Tail;
    if (Queue->Tail) Queue->Tail->GlobalNext = Tcb;
    else Queue->Head = Tcb;
    Queue->Tail = Tcb;
}

void ThreadQueueRemove(struct KeSchedQueue* Queue, ThreadCtrlBlk* Tcb) {
    if (Tcb->GlobalPrev) Tcb->GlobalPrev->GlobalNext = Tcb->GlobalNext;
    else Queue->Head = Tcb->GlobalNext;
    if (Tcb->GlobalNext) Tcb->GlobalNext->GlobalPrev = Tcb->GlobalPrev;
    else Queue->Tail = Tcb->GlobalPrev;
}

// picks the next thread
ThreadCtrlBlk* ThreadNext(struct KeScheduler* Sched) {
    if (Sched->Bitmap == 0) return IdleThreadPtr;
    uint32_t Priority = 63 - __builtin_clzll(Sched->Bitmap);
    // printf("Priority = %d\r\n", Priority);
    ThreadCtrlBlk* Thr = Sched->Queues[Priority].Head;
    // if (Thr == NULL) printf("ThreadNext: picked Priority %d but queue is empty (bitmap=0x%lx)", Priority, Sched->Bitmap);
    ThreadQueueRemove(&Sched->Queues[Priority], Thr);
    if (Sched->Queues[Priority].Head == NULL) {
        Sched->Bitmap &= ~(1ULL << Priority);
    }
    // printf("picked and returning thread 0x%lx {tid=%d, proc{pid=%d, name='%s'}, priority=%d}\r\n", Thr, Thr->tid, Thr->ParentProc->pid, Thr->ParentProc->name, Thr->Priority);
    return Thr;
}

void ThreadAdd(ThreadCtrlBlk* Tcb) {
    KeScheduler* Scheduler = KernelGetInformation()->Scheduler[Tcb->CpuNum];
    if (Scheduler) {
        // printf("adding thread 0x%lx with tid %d owned by pid %d\r\n", Tcb, Tcb->tid, Tcb->ParentProc->pid);
        ThreadQueueAdd(&Scheduler->Queues[Tcb->Priority], Tcb);
        Scheduler->Bitmap |= (1ULL << Tcb->Priority);
        Tcb->State = SCHED_THREAD_READY;
    }
}

void ThreadRemove(ThreadCtrlBlk* Tcb) {
    KeScheduler* Scheduler = KernelGetInformation()->Scheduler[Tcb->CpuNum];
    if (Scheduler) {
        KDBG;
        ThreadQueueRemove(&Scheduler->Queues[Tcb->Priority], Tcb);
        if (Scheduler->Queues[Tcb->Priority].Head == NULL) {
            Scheduler->Bitmap &= ~(1ULL << Tcb->Priority);
        }
    }
}


void ThreadWake(ThreadCtrlBlk* Tcb) {
    if (!Tcb) return;

    uint64_t r = SpnLckAcquireRfl(&SchedSpinlock);

    Tcb->State = SCHED_THREAD_READY;
    Tcb->GlobalNext = NULL;

    ThreadAdd(Tcb);
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

static Spinlock DeathSpinlock = {ATOMIC_FLAG_INIT};

void ThrDeathCleanup() {
    uint64_t r = SpnLckAcquireRfl(&DeathSpinlock);
    ThreadCtrlBlk* c = DeathQueue.Head;
    DeathQueue.Head = NULL;
    DeathQueue.Tail = NULL;
    while (c != NULL) {
        ThreadCtrlBlk* n = c->DeathNext;
        // printf("cleaning up thread 0x%lx {tid=%d, pid=%d}\r\n", c, c->tid, c->ParentProc->pid);
        if (c->KernelStackBase) {
            MmFree((void*)c->KernelStackBase);
        }
        if (c->UserStackBase) {
            PmmFreePages((void*)V2P(c->UserStackBase), PS_USER_STACK_PAGES);
        }
        c->ParentProc->threads--;
        if(c->ParentProc->threads <= 0) {
            printf("todo: remove process from list..\r\n");
            _s:
            KernelUnlockRsLck();
            MmFree(c->ParentProc->FileHandleTable);
            ProcFreePML4(c->ParentProc->pml4);
            MmFree(c->ParentProc->ThreadList);
            if (c->ParentProc->TtyObj) MmFree(c->ParentProc->TtyObj);
            MmFree(c->ParentProc);
        }
        MmFree(c);
        c = n;
    }
    SpnLckReleaseRfl(&DeathSpinlock, r);
}

// caller should preempt if Tcb == CurrentThread
// see SysExit
void ThrDeathMark(ThreadCtrlBlk* Tcb) {
    uint64_t r = SpnLckAcquireRfl(&DeathSpinlock);
    if (Tcb->State == SCHED_THREAD_READY) {
        ThreadRemove(Tcb);
    }
    Tcb->State = SCHED_THREAD_DYING;
    KeSchedQueue* Queue = &DeathQueue;
    THR_DEATHADD(Queue, Tcb);
    // printf("marked thread 0x%lx {tid = %d, pid=%d} for death.\r\n", Tcb, Tcb->tid, Tcb->ParentProc->pid);
    SpnLckReleaseRfl(&DeathSpinlock, r);
    // if (Tcb == CurrentThread) {
    //     Schedule(KernelGetInformation()->Scheduler[Tcb->CpuNum]);
    // } else {
    //     // ... 
    // }
}

void ThreadEntry() {
    // SpnLckRelease(&SchedSpinlock);
    ThrDeathCleanup();
    HAL_INT_ON();
    // printf("sched: wrapper: entering thread(tid=%d pid=%d, entry=0x%lx)\r\n", CurrentThread->tid, CurrentThread->ParentProc->pid, CurrentThread->entry);
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
    KeSchedQueue* Queue = proc->ThreadList;
    PROC_THRADD(Queue, tcb);
    proc->threads++;
    tcb->tid = proc->threads-1;
}