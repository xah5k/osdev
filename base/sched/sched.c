#include "arch/x86_64/cpu/cpu.h"
#include "arch/x86_64/cpu/paging.h"
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
#ifdef __x86_64__
#include <arch/x86_64/hal.h>
#endif
Spinlock SchedSpinlock = {ATOMIC_FLAG_INIT};

ThreadCtrlBlk* CurrentThread;
ThreadCtrlBlk* ReadyQueueHead;
ThreadCtrlBlk* DeathThread;
ThreadCtrlBlk* IdleThreadPtr;
static KernelInformation* gkinfoPtr;
void SchedIdleThread() {
    while (1) asm("sti; hlt");
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

        asm volatile("cli");
        #ifdef __x86_64__
        gkinfoPtr->tss->rsp0 = NextThr->KernelRsp;
        CpuWriteMsr(0xC0000100, NextThr->FsBase);
        CpuWriteMsr(0xC0000102, NextThr->GsBase);
        SpnLckReleaseRfl(&SchedSpinlock, r);
        HalContextSw(&OldThr->KernelRsp, NextThr->KernelRsp);
        #endif
    }
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
    asm volatile("sti");
}

void SchedYield() {
    Schedule();
}
KE_EXPORT_SYMBOL(SchedYield);