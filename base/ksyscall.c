#include <ksyscall.h>
#ifdef __x86_64__
#include <arch/x86_64/archsyscall.h>
#endif
#include <sched/process.h>
#include <sched/sched.h>
#include <util/util.h>
#include "util/spinlock.h"
#include <printfwrapper.h>
extern Spinlock SchedSpinlock;

extern ThreadCtrlBlk* CurrentThread;
extern ThreadCtrlBlk* ReadyQueueHead;
extern ThreadCtrlBlk* DeathThread;

uint64_t SysExit(uint64_t exitcode, KE_SYSCALL_ARGS_UNUSED1) {
    printf("ksyscall: SysExit: exit current thread with code %d\r\n", exitcode);
    // handle exit
    SpnLckAcquire(&SchedSpinlock);
    CurrentThread->exitcode = exitcode;
    //printf("process: handling exit of thread(tid=%d, belonging to pid %d)\r\n", CurrentThread->tid, CurrentThread->ParentProc->pid);
    ThreadCtrlBlk* c = CurrentThread;

    // remove it from process list of threads

    if (c == c->ParentProc->ThreadListHead) {
        c->ParentProc->ThreadListHead = c->ParentProc->ThreadListHead->ProcNext;
        c->ParentProc->threads--;
        goto _2;
    }
    ThreadCtrlBlk* current1 = c->ParentProc->ThreadListHead;
    ThreadCtrlBlk* previous1 = NULL;
    while (current1 != NULL) {
        if (current1 == c) {
            break;
        }
        previous1 = current1;
        current1 = current1->ProcNext;
    }

    KATTEMPT(current1);
    KATTEMPT(current1 == c);
    // unlink from list
    previous1->ProcNext = current1->ProcNext;
    _2:
    if (c != CurrentThread) {
        if (c == ReadyQueueHead) {
            ReadyQueueHead = c->GlobalNext;
            c->GlobalNext = NULL;
            goto _3;
        }
        
        ThreadCtrlBlk* current2 = ReadyQueueHead;
        ThreadCtrlBlk* previous2 = NULL;
        while (current2 != NULL && current2 != c) {
            //printf("current2=0x%lx\r\n", current2);
            previous2 = current2;
            current2 = current2->GlobalNext;
        }

        if (!current2 || current2 != c) {
           // printf("current2=0x%lx c=0x%lx\r\n", current2, c);
            KdBugcheck2(KERNEL_CORE_COMP_FAIL, NULL, __LINE__, __FILE__);
        }
        previous2->GlobalNext = current2->GlobalNext;
        current2->GlobalNext = NULL;
    }
    _3:
    c->ProcNext = NULL;
    c->GlobalNext = NULL;
    DeathThread = c;
    SpnLckRelease(&SchedSpinlock);
    SchedYield();
}

void KeRegisterSyscalls() {
    KiRegisterSyscall(OS_EXIT, SysExit);
}