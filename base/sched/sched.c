#include "fs/vfs.h"
#include "kernel.h"
#include "mm/heap.h"
#include <mm/pmm.h>
#include <sched/sched.h>
#include <stddef.h>
#include <stdint.h>
#include <util/spinlock.h>
#include <printfwrapper.h>
#include <memory.h>
#include <sched/process.h>
#include <kedriver.h>
#include <hal/hal.h>
#include <hal/mmu.h>
#include <hal/ps.h>

Spinlock SchedSpinlock = {ATOMIC_FLAG_INIT};

ThreadCtrlBlk* CurrentThread;
KeSchedQueue DeathQueue = {NULL, NULL};
ThreadCtrlBlk* IdleThreadPtr;
static KernelInformation* gkinfoPtr;
void SchedIdleThread() {
    HAL_HALT_WITHINT();
}

void SchedInitalize(KernelInformation* kinfo) {
    KeScheduler* Scheduler = MmAllocate(sizeof(KeScheduler));
    memset((void*)Scheduler, 0, sizeof(KeScheduler));
    kinfo->Scheduler[kinfo->CurrentSchedCount] = Scheduler;
    kinfo->CurrentSchedCount++;

    ProcessCtrlBlk* KernelProc = (ProcessCtrlBlk*)MmAllocate(sizeof(ProcessCtrlBlk));
    memcpy((void*)KernelProc->Name, (void*)"Kernel Process", sizeof("Kernel Process")+1);
    memcpy((void*)KernelProc->Cwd, (void*)"initrd:/boot", 13);
    KernelProc->pml4 = (virtaddr*)HalGetPageTable();
    KernelProc->cr3 = (uint64_t)KernelProc->pml4;
    KernelProc->Pid = 0;
    KernelProc->NextFh = 3; // process.c
    KernelProc->FileHandleTable = MmAllocate(sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    KernelProc->ThreadList = MmAllocate(sizeof(KeSchedQueue));
    memset(KernelProc->FileHandleTable, 0, sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    KernelProc->Next = NULL;

    // create kernel thread
    ThreadCtrlBlk* KernelThread = (ThreadCtrlBlk*)MmAllocate(sizeof(ThreadCtrlBlk));
    memset(KernelThread, 0, sizeof(ThreadCtrlBlk));
    KernelThread->Tid = 0;
    KernelThread->State = SCHED_THREAD_RUNNING;
    KernelThread->KernelRsp = HalGetStack();
    KernelThread->Privilege = SCHED_PRIV_KERNEL;
    KernelThread->Priority = 4;
    KernelThread->Bpriority = 4;
    KernelThread->TickDefault = 64;
    KernelThread->Deadline = 64;

    // create idle thread
    ThreadCtrlBlk* IdleThread = ThreadNew(SchedIdleThread, SCHED_PRIV_KERNEL, SCHED_THREAD_PLOW, 0, 0, 0, 0);
    ProcAttachThread(KernelProc, KernelThread);
    ProcAttachThread(KernelProc, IdleThread);

    CurrentThread = KernelThread;
    ThreadAdd(KernelThread);
    ThreadAdd(IdleThread);
    IdleThreadPtr = IdleThread;
    // add kernel process to list of proccesses
    gkinfoPtr = kinfo;
    gkinfoPtr->ProcessListHead = KernelProc;
    gkinfoPtr->CurrentProcess = KernelProc;
    gkinfoPtr->KernelProcess = KernelProc;
}
void Schedule(KeScheduler* Sched) {
    uint64_t r = SpnLckAcquireRfl(&SchedSpinlock);
    ThreadCtrlBlk* OldThr = CurrentThread;
    if (OldThr->State == SCHED_THREAD_DYING) {
        OldThr->State = SCHED_THREAD_DEAD;
    } else if (OldThr->State == SCHED_THREAD_RUNNING) {
        OldThr->State = SCHED_THREAD_READY;
    }

    if (OldThr->State == SCHED_THREAD_READY) {
        OldThr->Deadline = OldThr->TickDefault;
        ThreadQueueAdd(&Sched->Queues[OldThr->Priority], OldThr);
        Sched->Bitmap |= (1ULL << OldThr->Priority);
    }
    ThreadCtrlBlk* NextThr = ThreadNext(Sched);
    if (!NextThr) NextThr = IdleThreadPtr;
    CurrentThread = NextThr;
    NextThr->State = SCHED_THREAD_RUNNING;

    if (OldThr != NextThr) {
        if (NextThr->ParentProc != OldThr->ParentProc) {
            gkinfoPtr->CurrentProcess = NextThr->ParentProc;
        }

        if (NextThr->ParentProc->cr3 != OldThr->ParentProc->cr3) {
            if (NextThr->ParentProc->cr3) HalSwPageTable(NextThr->ParentProc->cr3);
            else {
                printf("sched: warn: cr3 of next process is NULL?\r\n");
                printf("sched: warn: OldThr=0x%lx NextThr=0x%lx NextThr->ParentProc{pid=%d, cr3=0x%lx} OldThr->ParentProc{pid=%d, cr3=0x%lx}\r\n", OldThr, NextThr, NextThr->ParentProc->Pid, NextThr->ParentProc->cr3, OldThr->ParentProc->Pid, OldThr->ParentProc->cr3);
            }
        }

        HAL_INT_OFF();
        HalContextSwPrep(NextThr);
        SpnLckReleaseRfl(&SchedSpinlock, r);
        HalContextSw(&OldThr->KernelRsp, NextThr->KernelRsp);
    } else {
        SpnLckReleaseRfl(&SchedSpinlock, r);
    }
    SpnLckReleaseRfl(&SchedSpinlock, r);
    HAL_INT_ON();
    ThrDeathCleanup();
}

void SchedYield() {
    Schedule(KernelGetInformation()->Scheduler[0]);
}
KE_EXPORT_SYMBOL(SchedYield);