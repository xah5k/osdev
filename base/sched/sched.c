#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/cpu/paging.h"
#include "fs/vfs.h"
#include "kernel.h"
#include "mm/heap.h"
#include <sched/sched.h>
#include <stddef.h>
#include <stdint.h>
#include <util/spinlock.h>
#include <printfwrapper.h>
#include <memory.h>
#include <sched/process.h>

Spinlock SchedSpinlock = {ATOMIC_FLAG_INIT};

ThreadCtrlBlk* CurrentThread;
ThreadCtrlBlk* ReadyQueueHead;
ThreadCtrlBlk* DeathThread;
static KernelInformation* gkinfoPtr;
void SchedIdleThread() {
    while (1) asm("hlt");
}

void SchedInitalize(KernelInformation* kinfo) {
    ProcessCtrlBlk* KernelProc = (ProcessCtrlBlk*)MmAllocate(sizeof(ProcessCtrlBlk));
    KernelProc->pml4 = (virtaddr*)_x86_64_get_pml4();
    KernelProc->cr3 = MmuGetPhys((virtaddr)KernelProc->cr3);
    KernelProc->pid = 0;
    KernelProc->nextfh = 0;
    KernelProc->FileHandleTable[0] = MmAllocate(sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    KernelProc->Next = NULL;

    // create kernel thread
    ThreadCtrlBlk* KernelThread = (ThreadCtrlBlk*)MmAllocate(sizeof(ThreadCtrlBlk));
    memset(KernelThread, 0, sizeof(ThreadCtrlBlk));
    KernelThread->tid = 0;
    KernelThread->state = SCHED_THREAD_RUNNING;
    KernelThread->rsp = _x86_64_get_stack();
    //KernelThread->ParentProc = KernelProc;

    ThreadCtrlBlk* IdleThread = (ThreadCtrlBlk*)MmAllocate(sizeof(ThreadCtrlBlk));
    memset(IdleThread, 0, sizeof(ThreadCtrlBlk));
    ThreadCreate(IdleThread, SchedIdleThread);
    IdleThread->tid = 1;
    IdleThread->state = SCHED_THREAD_READY;
    //IdleThread->ParentProc = KernelProc;

    ProcAttachThread(KernelProc, KernelThread);
    ProcAttachThread(KernelProc, IdleThread);

    CurrentThread = KernelThread;
    ReadyQueueHead = IdleThread;

    // add kernel process to list of proccesses
    gkinfoPtr = kinfo;
    gkinfoPtr->ProcessListHead = KernelProc;
    gkinfoPtr->CurrentProcess = KernelProc;
}

void Schedule() {
    SpnLckAcquire(&SchedSpinlock);
    
    if (ReadyQueueHead == NULL) {
        SpnLckRelease(&SchedSpinlock);
        return;
    }

    ThreadCtrlBlk* OldThr = CurrentThread;
    ThreadCtrlBlk* NextThr = ReadyQueueHead;

    ReadyQueueHead = ReadyQueueHead->GlobalNext;
    NextThr->GlobalNext = NULL;

    OldThr->state = SCHED_THREAD_READY;
    if (OldThr != DeathThread) {
        OldThr->state = SCHED_THREAD_READY;
        if (ReadyQueueHead == NULL) {
            ReadyQueueHead = OldThr;
        } else {
            ThreadCtrlBlk* LastThr = ReadyQueueHead;
            while (LastThr->GlobalNext != NULL) {
                LastThr = LastThr->GlobalNext;
            }
            LastThr->GlobalNext = OldThr;
            OldThr->GlobalNext = NULL;
        }
    } else {
        OldThr->state = SCHED_THREAD_DEAD;
    }    
        

    CurrentThread = NextThr;
    NextThr->state = SCHED_THREAD_RUNNING;

    if (NextThr->ParentProc != OldThr->ParentProc) {
        gkinfoPtr->CurrentProcess = NextThr->ParentProc;
    }

    if (NextThr->ParentProc->cr3 != OldThr->ParentProc->cr3) {
        _x86_64_load_pml4(NextThr->ParentProc->cr3);
    }
    asm ("cli");
    #ifdef __x86_64__
    _x86_64_ctxswitch(&OldThr->rsp, NextThr->rsp);
    #endif
    if (DeathThread != NULL) {
        if (DeathThread->StackBase) {
            MmFree((void*)DeathThread->StackBase);
        }
        MmFree(DeathThread);
        DeathThread = NULL;
    }
    //printf("sched: thread with tid %d is back\r\n", CurrentThread->tid);
    SpnLckRelease(&SchedSpinlock);
}

void SchedYield() {
    Schedule();
}