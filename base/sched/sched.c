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
    memcpy((void*)KernelProc->name, (void*)"Kernel Process", sizeof("Kernel Process")+1);
    KernelProc->pml4 = (virtaddr*)_x86_64_get_pml4();
    KernelProc->cr3 = (uint64_t)KernelProc->pml4;
    KernelProc->pid = 0;
    KernelProc->nextfh = 0;
    KernelProc->FileHandleTable = MmAllocate(sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    memset(KernelProc->FileHandleTable, 0, sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    KernelProc->Next = NULL;

    // create kernel thread
    ThreadCtrlBlk* KernelThread = (ThreadCtrlBlk*)MmAllocate(sizeof(ThreadCtrlBlk));
    memset(KernelThread, 0, sizeof(ThreadCtrlBlk));
    KernelThread->tid = 0;
    KernelThread->state = SCHED_THREAD_RUNNING;
    KernelThread->KernelRsp = _x86_64_get_stack();
    KernelThread->privilege = SCHED_PRIV_KERNEL;

    // create idle thread
    ThreadCtrlBlk* IdleThread = ThreadNew(SchedIdleThread, SCHED_PRIV_KERNEL, 0, 0);

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
        //printf("NextThr->ParentProc = 0x%lx OldThr->ParentProc = 0x%lx\r\n", NextThr->ParentProc, OldThr->ParentProc);
        gkinfoPtr->CurrentProcess = NextThr->ParentProc;
    }

    if (NextThr->ParentProc->cr3 != OldThr->ParentProc->cr3) {
        //printf("NextThr->ParentProc->cr3 = 0x%lx OldThr->ParentProc->cr3 = 0x%lx\r\n", NextThr->ParentProc->cr3, OldThr->ParentProc->cr3);
        _x86_64_load_pml4(NextThr->ParentProc->cr3);
    }
    asm ("cli");
    #ifdef __x86_64__
    gkinfoPtr->tss->rsp0 = NextThr->KernelRsp;
    _x86_64_ctxswitch(&OldThr->KernelRsp, NextThr->KernelRsp);
    #endif
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
    //printf("sched: thread with tid %d is back\r\n", CurrentThread->tid);
    SpnLckRelease(&SchedSpinlock);
}

void SchedYield() {
    Schedule();
}