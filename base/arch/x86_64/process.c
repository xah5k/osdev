#include "process.h"
#include <sched/process.h>
#include "kernel.h"
#include "util/spinlock.h"
#include <mm/heap.h>
#include <memory.h>
#include <printfwrapper.h>
#include <sched/sched.h>
#include <mm/pmm.h>
#include <ksyscall.h>
#include <util/util.h>
#include <kedriver.h>
#include <sched/ipc/signal.h>
#include <external/posix/signal.h>
#include <sched/ipc/signaltrampoline.h>
#include <hal/mmu.h>
extern Spinlock SchedSpinlock;
extern ThreadCtrlBlk* CurrentThread;
extern ThreadCtrlBlk* ReadyQueueHead;
extern ThreadCtrlBlk* DeathThread;

uint64_t* ProcNewPML4() {
    uint64_t* NewPML4Phys = PmmAllocate();
    uint64_t* NewPML4 = (uint64_t*)((uint64_t)NewPML4Phys + gMmuVOffset);
    memset(NewPML4, 0, PAGE_SIZE);
    uint64_t* KernelPML4 = (uint64_t*)((uint64_t)_x86_64_get_pml4() + gMmuVOffset);
    memcpy(&NewPML4[256], &KernelPML4[256], 256 * sizeof(uint64_t));
    return NewPML4Phys;
}

// frees everything except pml4p itself
void ProcFreeInnerPML4(pagetable* pml4p) {
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
                            pd[k] = 0x0;
                            PmmFree((void*)pt_phys);
                        }
                    }
                    pdpt[j] = 0x0;
                    PmmFree((void*)(pdpt[j] & ~0xFFF));
                }
            }
            pml4[i] = 0x0;
            PmmFree((void*)(pml4[i] & ~0xFFF));
        }
    }
}
void ProcFreePML4(pagetable* pml4p) {
    ProcFreeInnerPML4(pml4p);
    PmmFree((void*)pml4p);
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

void ThreadCreateUserStack(ThreadCtrlBlk* Tcb, void* entry, const char** argv, int argc, const char** envp, int envc) {
    uint64_t StackSize = PS_USER_STACK_PAGES * PAGE_SIZE;
    uint64_t* StackBasePhys = (uint64_t*)PmmAllocatePages(PS_USER_STACK_PAGES);

    Tcb->UserStackBase = (uint64_t)P2V(StackBasePhys);

    uint64_t UserStackTop = PS_USER_STACK_BASE;
    uint64_t LocalVirt  = UserStackTop;
    uint64_t KernelVirt = Tcb->UserStackBase + StackSize;

    uint64_t* UserArgvAddresses = MmAllocate(sizeof(uint64_t) * argc);
    uint64_t* UserEnvpAddresses = MmAllocate(sizeof(uint64_t) * envc);

    // argv
    for (int i = argc - 1; i >= 0; i--) {
        uint64_t len = strlen(argv[i]) + 1;
        LocalVirt  -= len;
        KernelVirt -= len;
        memcpy((void*)KernelVirt, argv[i], len);
        UserArgvAddresses[i] = LocalVirt;
    }

    // envp
    for (int i = envc - 1; i >= 0; i--) {
        uint64_t len = strlen(envp[i]) + 1;
        LocalVirt  -= len;
        KernelVirt -= len;
        memcpy((void*)KernelVirt, envp[i], len);
        UserEnvpAddresses[i] = LocalVirt;
    }
    // todo: ?
    uint64_t align8 = LocalVirt % 8;
    LocalVirt  -= align8;
    KernelVirt -= align8;

    uint64_t block_words = 1 + (argc + 1) + (envc + 1) + (2 * 2);
    uint64_t block_bytes = block_words * sizeof(uint64_t);

    uint64_t block_start = LocalVirt - block_bytes;
    block_start &= ~0xFULL;

    uint64_t delta = LocalVirt - block_start;
    LocalVirt  -= delta;
    KernelVirt -= delta;

    uint64_t* kw = (uint64_t*)KernelVirt;
    int idx = 0;

    kw[idx++] = (uint64_t)argc;
    // fix here
    for (int i = 0; i < argc; i++)
        kw[idx++] = UserArgvAddresses[i];
    kw[idx++] = 0;

    for (int i = 0; i < envc; i++)
        kw[idx++] = UserEnvpAddresses[i];
    kw[idx++] = 0;

    kw[idx++] = 6;
    kw[idx++] = PAGE_SIZE; // depends on 2mb pages but we dont use those yet
    kw[idx++] = 0;
    kw[idx++] = 0;

    MmFree(UserArgvAddresses);
    MmFree(UserEnvpAddresses);

    Tcb->UserRsp  = LocalVirt;
    Tcb->UserArgc = argc;
}

void ThreadMapUserStack(ThreadCtrlBlk* Tcb) {
    if (!Tcb->ParentProc) return; // no parent proc page tables to map
    if (!Tcb->UserStackBase) return;
    uint64_t StackBasePhys = V2P(Tcb->UserStackBase);
    uint64_t StackSize = PS_USER_STACK_PAGES * PAGE_SIZE;
    uint64_t PhysLimit = StackBasePhys + StackSize;
    uint64_t StartVirt =  PS_USER_STACK_BASE - (StackSize);
    uint64_t CurrentVirt = StartVirt;
    for (uint64_t phys = StackBasePhys; phys < PhysLimit; phys += PAGE_SIZE) {
        MmuMapPage((pagetable*)P2V(Tcb->ParentProc->cr3), CurrentVirt, phys, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_US_USER);
        CurrentVirt += PAGE_SIZE;
    }
    // also map signal trampoline
    void* SignalTrampoline = PmmAllocate();
    memset((void*)P2V(SignalTrampoline), 0, MMU_PAGE_SIZE);
    memcpy((void*)P2V(SignalTrampoline), __signal_trampoline, sizeof(__signal_trampoline));
    MmuMapPage((pagetable*)P2V(Tcb->ParentProc->cr3), KE_LDR_SIGNAL_ADDR, (physaddr)SignalTrampoline, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_US_USER);
    // for (int i = 0; i < 10; i++) printf("thrmap: SignalTrampoline(mapped into proc cr3)[%d]=%x\r\n", i, vtmp[i]);
}
