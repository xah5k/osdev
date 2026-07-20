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

void ThreadCreateUserStack(ThreadCtrlBlk* Tcb, void* entry) {
    uint64_t* StackBasePhys = (uint64_t*)PmmAllocatePages(PS_USER_STACK_PAGES);
    uint64_t StackSize = PS_USER_STACK_PAGES * PAGE_SIZE;
    Tcb->UserStackBase = (uint64_t)P2V(StackBasePhys); 
    uint64_t UserStartVirt = PS_USER_STACK_BASE - (StackSize - PAGE_SIZE);
    uint64_t UserStackTop = UserStartVirt + StackSize;
    if (UserStackTop % 16 != 0) {
        UserStackTop -= 8;
    }
    Tcb->UserRsp = UserStackTop; 
}

void ThreadMapUserStack(ThreadCtrlBlk* Tcb) {
    if (!Tcb->ParentProc) return; // no parent proc page tables to map
    if (!Tcb->UserStackBase) return;
    uint64_t StackBasePhys = V2P(Tcb->UserStackBase);
    uint64_t StackSize = PS_USER_STACK_PAGES * PAGE_SIZE;
    uint64_t PhysLimit = StackBasePhys + StackSize;
    uint64_t StartVirt =  PS_USER_STACK_BASE - (StackSize - PAGE_SIZE);
    uint64_t CurrentVirt = StartVirt;
    for (uint64_t phys = StackBasePhys; phys < PhysLimit; phys += PAGE_SIZE) {
        MmuMapPage((pagetable*)P2V(Tcb->ParentProc->cr3), CurrentVirt, phys, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_US_USER);
        CurrentVirt += PAGE_SIZE;
    }
}

ThreadCtrlBlk* ThreadNew(void* entry, uint8_t priv) {
    ThreadCtrlBlk* new = MmAllocate(sizeof(ThreadCtrlBlk));
    memset(new, 0, sizeof(ThreadCtrlBlk));
    new->state = SCHED_THREAD_READY;
    //new->tid = ThreadGetTid();
    new->privilege = priv;
    ThreadCreateKrnlStack(new, entry);
    if (priv > SCHED_PRIV_KERNEL) {
        ThreadCreateUserStack(new, entry);
    }
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
    new->FileHandleTable = MmAllocate(sizeof(VfsOpenFileDescr) * VFS_MAX_ALLOWED_OPEN_HANDLES);
    new->Next = NULL;
    return new;
}

void ProcessCreate(void* entry, KernelInformation* kinfo, uint8_t priv) {
    ProcessCtrlBlk* proc = ProcessNew();
    ThreadCtrlBlk* thr = ThreadNew(entry, priv);
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
                asm volatile ("cli");
                _x86_64_usjmp((uint64_t)entry, CurrentThread->UserRsp);
                break;
            }
        }
    }
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