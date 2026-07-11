#include "process.h"
#include "kernel.h"
#include "util/spinlock.h"
#include <mm/heap.h>
#include <memory.h>
#include <printfwrapper.h>
#include "sched.h"
#include <mm/pmm.h>
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
    uint64_t* NewPML4 = PmmAllocate();
    memset(NewPML4, 0, PAGE_SIZE);
    uint64_t* KernelPML4 = (uint64_t*)_x86_64_get_pml4();
    memcpy(&NewPML4[256], &KernelPML4[256], 256 * sizeof(uint64_t));
    return NewPML4;
}

void ProcListRunning(KernelInformation* kinfo) {
    ProcessCtrlBlk* list = kinfo->ProcessListHead;
    if (!list) return;
    ProcessCtrlBlk* current = list;
    printf("process: List of running processes: \r\n");
    while (current != NULL) {
        printf("process:     [*] PID %d\r\n", current->pid);
        ThreadCtrlBlk* thrc = current->ThreadListHead;
        while (thrc != NULL) {
            printf("process:            [*] TID %d\r\n", thrc->tid);
            thrc = thrc->ProcNext;
        }
        current = current->Next;
    }
}
void ThreadCreateStack(ThreadCtrlBlk* Tcb, void* entry) {
    uint64_t* StackBase = (uint64_t*)MmAllocate(16384);
    uint64_t* StackTop = StackBase + (16384 / 8);
    //Tcb->StackBase = (uint64_t)StackTop - (16384 * 8);
    if ((uint64_t)StackTop % 16 != 0) StackTop--;
    *(--StackTop) = (uint64_t)ThreadEntry;

    for (int i = 0; i < 6; i++) {
        *(--StackTop) = 0;
    }
    *(--StackTop) = 0x202;
    Tcb->rsp = (uint64_t)StackTop;
    Tcb->StackBase = (uint64_t)StackBase;
    Tcb->entry = entry;
}

ThreadCtrlBlk* ThreadNew(void* entry) {
    ThreadCtrlBlk* new = MmAllocate(sizeof(ThreadCtrlBlk));
    memset(new, 0, sizeof(ThreadCtrlBlk));
    new->state = SCHED_THREAD_READY;
    //new->tid = ThreadGetTid();
    ThreadCreateStack(new, entry);
    return new;
}

// creates a new process and it makes one thread with the entry point
ProcessCtrlBlk* ProcessNew() {
    ProcessCtrlBlk* new = MmAllocate(sizeof(ProcessCtrlBlk));
    new->pml4 = ProcNewPML4();
    new->cr3 = (uint64_t)new->pml4;
    new->pid = ProcGetPid()+1;
    new->nextfh = 0;
    new->threads = 0;
    new->FileHandleTable[0] = MmAllocate(sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    new->Next = NULL;
}

void ProcessCreate(void* entry, KernelInformation* kinfo) {
    ProcessCtrlBlk* proc = ProcessNew();
    ThreadCtrlBlk* thr = ThreadNew(entry);
    ProcAttachThread(proc, thr);
    ThreadAdd(thr);
    proc->Next = kinfo->ProcessListHead;
    kinfo->ProcessListHead = proc;
    kinfo->CurrentProcess = proc;
}

void ThreadAdd(ThreadCtrlBlk* Tcb) {
    Tcb->GlobalNext = ReadyQueueHead;
    ReadyQueueHead = Tcb;
}

void ThreadEntry() {
    SpnLckRelease(&SchedSpinlock);
    if (DeathThread != NULL) {
        if (DeathThread->StackBase) {
            MmFree((void*)DeathThread->StackBase);
        }
        if(DeathThread->ParentProc->threads <= 0) {
            // remove from kernel list
            KernelInformation* kinfo = KernelGetInformation();
            ProcessCtrlBlk* ProcList = kinfo->ProcessListHead;
            if (DeathThread->ParentProc == ProcList){ kinfo->ProcessListHead = kinfo->ProcessListHead->Next; } else {
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
            MmFree(DeathThread->ParentProc->FileHandleTable);
            PmmFree(DeathThread->ParentProc->pml4);
            MmFree(DeathThread->ParentProc);
        }
        MmFree(DeathThread);
        DeathThread = NULL;
    }
    asm volatile ("sti");
    //printf("sched: wrapper: entering thread(tid=%d, entry=0x%lx)\r\n", CurrentThread->tid, CurrentThread->entry);
    void (*entry)() = CurrentThread->entry;
    if (entry) entry();
    // handle exit
    SpnLckAcquire(&SchedSpinlock);
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
    while (1) {asm("hlt");}
}

void ProcAttachThread(ProcessCtrlBlk* proc, ThreadCtrlBlk* tcb) {
    tcb->ParentProc = proc;
    tcb->ProcNext = proc->ThreadListHead;
    proc->ThreadListHead = tcb;
    proc->threads++;
    tcb->tid = proc->threads-1;
}