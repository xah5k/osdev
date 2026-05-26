#include "process.h"
#include "kernel.h"
#include "util/spinlock.h"
#include <mm/heap.h>
#include <memory.h>
#include <printfwrapper.h>
extern Spinlock SchedSpinlock;
extern ThreadCtrlBlk* CurrentThread;

uint64_t* ProcNewPML4() {
    uint64_t* NewPML4 = (uint64_t*)MmAllocate(PAGE_SIZE);
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
        printf("process: PID %d\r\n", current->pid);
        printf("process:    Threads:\r\n");
        ThreadCtrlBlk* thrc = current->ThreadListHead;
        while (thrc != NULL) {
            printf("process:    TID %d\r\n", thrc->tid);
            thrc = thrc->ProcNext;
        }
        current = current->Next;
    }
}
void ThreadCreate(ThreadCtrlBlk* Tcb, void* entry) {
    uint64_t* StackTop = (uint64_t*)MmAllocate(16384) + (16384 / 8);
    //Tcb->StackBase = (uint64_t)StackTop - (16384 * 8);
    if ((uint64_t)StackTop % 16 != 0) StackTop--;
    *(--StackTop) = (uint64_t)ThreadEntry;

    for (int i = 0; i < 6; i++) {
        *(--StackTop) = 0;
    }
    *(--StackTop) = 0x202;
    Tcb->rsp = (uint64_t)StackTop;
    Tcb->entry = entry;
    //printf("sched: creating stack for thread. tid=%d actual entry=0x%lx wrapper entry=0x%lx\r\n", Tcb->tid, entry, ThreadEntry);
}
void ThreadEntry() {
    SpnLckRelease(&SchedSpinlock);
    asm volatile ("sti");
    //printf("sched: wrapper: entering thread(tid=%d, entry=0x%lx)\r\n", CurrentThread->tid, CurrentThread->entry);
    void (*entry)() = CurrentThread->entry;
    if (entry) entry();
    //printf("sched: wrapper: panic: thread exited!\r\n");
    while (1) {asm("hlt");}
}

void ProcAttachThread(ProcessCtrlBlk* proc, ThreadCtrlBlk* tcb) {
    tcb->ParentProc = proc;
    tcb->ProcNext = proc->ThreadListHead;
    proc->ThreadListHead = tcb;
}