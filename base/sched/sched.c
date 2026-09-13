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
ThreadCtrlBlk* ReadyQueueHead;
ThreadCtrlBlk* DeathThread;
ThreadCtrlBlk* IdleThreadPtr;
static KernelInformation* gkinfoPtr;
void SchedIdleThread() {
    HAL_HALT_WITHINT();
}

void SchedInitalize(KernelInformation* kinfo) {
    ProcessCtrlBlk* KernelProc = (ProcessCtrlBlk*)MmAllocate(sizeof(ProcessCtrlBlk));
    memcpy((void*)KernelProc->name, (void*)"Kernel Process", sizeof("Kernel Process")+1);
    memcpy((void*)KernelProc->cwd, (void*)"initrd:/boot", 13);
    KernelProc->pml4 = (virtaddr*)HalGetPageTable();
    KernelProc->cr3 = (uint64_t)KernelProc->pml4;
    KernelProc->pid = 0;
    KernelProc->nextfh = 3; // process.c
    KernelProc->FileHandleTable = MmAllocate(sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    memset(KernelProc->FileHandleTable, 0, sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    KernelProc->Next = NULL;

    // create kernel thread
    ThreadCtrlBlk* KernelThread = (ThreadCtrlBlk*)MmAllocate(sizeof(ThreadCtrlBlk));
    memset(KernelThread, 0, sizeof(ThreadCtrlBlk));
    KernelThread->tid = 0;
    KernelThread->state = SCHED_THREAD_RUNNING;
    KernelThread->KernelRsp = HalGetStack();
    KernelThread->privilege = SCHED_PRIV_KERNEL;

    // create idle thread
    ThreadCtrlBlk* IdleThread = ThreadNew(SchedIdleThread, SCHED_PRIV_KERNEL, 0, 0, 0, 0);

    ProcAttachThread(KernelProc, KernelThread);
    ProcAttachThread(KernelProc, IdleThread);

    CurrentThread = KernelThread;
    ReadyQueueHead = IdleThread;
    IdleThreadPtr = IdleThread;
    // add kernel process to list of proccesses
    gkinfoPtr = kinfo;
    gkinfoPtr->ProcessListHead = KernelProc;
    gkinfoPtr->CurrentProcess = KernelProc;
    gkinfoPtr->KernelProcess = KernelProc;
}
void Schedule() {
    uint64_t r = SpnLckAcquireRfl(&SchedSpinlock);

    if (ReadyQueueHead == NULL && CurrentThread->state == SCHED_THREAD_RUNNING) {
        SpnLckReleaseRfl(&SchedSpinlock, r);
        return;
    }
    ThreadCtrlBlk* OldThr = CurrentThread;

    if (OldThr == DeathThread) {
        OldThr->state = SCHED_THREAD_DEAD;
    } else if (OldThr->state == SCHED_THREAD_RUNNING) {
        OldThr->state = SCHED_THREAD_READY;
    }

    if (OldThr->state == SCHED_THREAD_READY) {
        OldThr->GlobalNext = NULL;
        if (ReadyQueueHead == NULL) {
            ReadyQueueHead = OldThr;
        } else {
            ThreadCtrlBlk* LastThr = ReadyQueueHead;
            while (LastThr->GlobalNext != NULL) {
                LastThr = LastThr->GlobalNext;
            }
            LastThr->GlobalNext = OldThr;
        }
    }

    ThreadCtrlBlk* PrevThr = NULL;
    ThreadCtrlBlk* NextThr = ReadyQueueHead;

    while (NextThr != NULL && NextThr->state != SCHED_THREAD_READY) {
        PrevThr = NextThr;
        NextThr = NextThr->GlobalNext;
    }

    if (NextThr != NULL) {
        if (PrevThr == NULL) {
            ReadyQueueHead = NextThr->GlobalNext;
        } else {
            PrevThr->GlobalNext = NextThr->GlobalNext;
        }
        NextThr->GlobalNext = NULL;
    } else {
        // deadass why and how would this even happen.
        NextThr = IdleThreadPtr; 
    }

    CurrentThread = NextThr;
    NextThr->state = SCHED_THREAD_RUNNING;

    if (OldThr != NextThr) {
        if (NextThr->ParentProc != OldThr->ParentProc) {
            gkinfoPtr->CurrentProcess = NextThr->ParentProc;
        }

        if (NextThr->ParentProc->cr3 != OldThr->ParentProc->cr3) {
            HalSwPageTable(NextThr->ParentProc->cr3);
        }

        HAL_INT_OFF();
        HalContextSwPrep(NextThr);
        SpnLckReleaseRfl(&SchedSpinlock, r);
        HalContextSw(&OldThr->KernelRsp, NextThr->KernelRsp);
    }
    ThrDeathCleanup();
    HAL_INT_ON();
}

void SchedYield() {
    Schedule();
}
KE_EXPORT_SYMBOL(SchedYield);