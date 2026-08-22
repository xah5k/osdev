#include "paging.h"
#include <mm/pmm.h>
#include <memory.h>
#include <stdint.h>
#include <kedriver.h>
#include <printfwrapper.h>
// offset used for memory
uint64_t gMmuVOffset = 0;
KE_EXPORT_SYMBOL(gMmuVOffset);
void MmuMapPage(pagetable* pml4, virtaddr virt, physaddr phys, unsigned int flags) {
    uint64_t pml4idx = (virt >> 39) & 0x1FF;
    uint64_t pdptidx = (virt >> 30) & 0x1FF;
    uint64_t pdidx = (virt >> 21) & 0x1FF;
    uint64_t ptidx = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4idx] & MMU_PAGE_BIT_P_PRESENT)){
        pagetable* newpdpt = PmmAllocate();
        memset(((uint8_t*)newpdpt + gMmuVOffset), 0, MMU_PAGE_SIZE); // pmmallocate doesnt zero out the page frame it returns
        pml4[pml4idx] = (uint64_t)newpdpt | MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_US_USER;
    }
    pagetable* pdpt = (pagetable*)(((uintptr_t)pml4[pml4idx] & ~0xFFFULL) + gMmuVOffset);

    if (!(pdpt[pdptidx] & MMU_PAGE_BIT_P_PRESENT)) {
        pagetable* newpd = PmmAllocate();
        memset(((uint8_t*)newpd + gMmuVOffset), 0, MMU_PAGE_SIZE);
        pdpt[pdptidx] = (uint64_t)newpd | flags;
    }

    pagetable* pd = (pagetable*)(((uintptr_t)pdpt[pdptidx] & ~0xFFFULL) + gMmuVOffset);

    if (!(pd[pdidx] & MMU_PAGE_BIT_P_PRESENT)) {
        pagetable* newpt = PmmAllocate();
        memset(((uint8_t*)newpt + gMmuVOffset), 0, MMU_PAGE_SIZE);
        pd[pdidx] = (uint64_t)newpt | flags;
    }

    pagetable* pt = (pagetable*)(((uintptr_t)pd[pdidx] & ~0xFFFULL) + gMmuVOffset);
    pt[ptidx] = phys | flags;
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}
KE_EXPORT_SYMBOL(MmuMapPage);

physaddr MmuGetPhys(uint64_t pml4p, virtaddr virt) {
    uint64_t cr3 = pml4p;
    uint64_t* pml4 = (uint64_t*)((cr3 & MMU_PAGE_ADDR_MASK) + gMmuVOffset);
    uint64_t pml4e = pml4[(virt >> 39) & 0x1FF];
    if (!(pml4e & 1)) return 0;

    uint64_t* pdpt = (uint64_t*)((pml4e & MMU_PAGE_ADDR_MASK) + gMmuVOffset);
    uint64_t pdpte = pdpt[(virt >> 30) & 0x1FF];
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & MMU_PAGE_ADDR_MASK & ~0x3FFFFFFFULL) + (virt & 0x3FFFFFFF);

    uint64_t* pd = (uint64_t*)((pdpte & MMU_PAGE_ADDR_MASK) + gMmuVOffset);
    uint64_t pde = pd[(virt >> 21) & 0x1FF];
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & MMU_PAGE_ADDR_MASK & ~0x1FFFFFULL) + (virt & 0x1FFFFF);

    uint64_t* pt = (uint64_t*)((pde & MMU_PAGE_ADDR_MASK) + gMmuVOffset);
    uint64_t pte = pt[(virt >> 12) & 0x1FF];
    if (!(pte & 1)) return 0;

    return (pte & MMU_PAGE_ADDR_MASK) + (virt & 0xFFF);
}
KE_EXPORT_SYMBOL(MmuGetPhys);

void MmuUnmapPage(pagetable* pml4, virtaddr virt) {
    uint64_t pml4idx = (virt >> 39) & 0x1FF;
    uint64_t pdptidx = (virt >> 30) & 0x1FF;
    uint64_t pdidx = (virt >> 21) & 0x1FF;
    uint64_t ptidx = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4idx] & MMU_PAGE_BIT_P_PRESENT)){
        return;
    }
    pagetable* pdpt = (pagetable*)((pml4[pml4idx] & ~0xFFF) + gMmuVOffset);

    if (!(pdpt[pdptidx] & MMU_PAGE_BIT_P_PRESENT)) {
        return;
    }

    pagetable* pd = (pagetable*)((pdpt[pdptidx] & ~0xFFF) + gMmuVOffset);

    if (!(pd[pdidx] & MMU_PAGE_BIT_P_PRESENT)) {
        return;
    }

    pagetable* pt = (pagetable*)((pd[pdidx] & ~0xFFF) + gMmuVOffset);
    if (pt[ptidx] & MMU_PAGE_BIT_P_PRESENT) {
        physaddr phys = (pt[ptidx] & ~0xFFF);
        pt[ptidx] = 0x0;
        asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
        PmmFree((void*)phys);
    }
}
KE_EXPORT_SYMBOL(MmuUnmapPage);

void MmuMapRegion(pagetable* pml4, virtaddr vstart, physaddr pstart, physaddr pend, unsigned int flags) {
    vstart &= ~0xFFFULL;
    pstart &= ~0xFFFULL;
    pend = (pend + 0xFFFULL) & ~0xFFFULL;
    for (uint64_t i = 0; i < (pend - pstart); i+=MMU_PAGE_SIZE) {
        MmuMapPage(pml4, vstart + i, pstart + i, flags);
    }
}
KE_EXPORT_SYMBOL(MmuMapRegion);
int MmuForkCopyUserSpace(pagetable* ParentPml4, pagetable* ChildPml4) {
    pagetable* ParentPml4V = (pagetable*)((uint64_t)ParentPml4 + gMmuVOffset);
    pagetable* ChildPml4V  = (pagetable*)((uint64_t)ChildPml4 + gMmuVOffset);

    for (int i4 = 0; i4 < 512; i4++) {
        if (i4 >= 256) continue;
        uint64_t Pml4Entry = ParentPml4V[i4];
        if (!(Pml4Entry & MMU_PAGE_BIT_P_PRESENT)) continue;
        pagetable* ParentPdpt = (pagetable*)((Pml4Entry & ~0xFFFULL) + gMmuVOffset);
        uint64_t ChildPdptPhys = (uint64_t)PmmAllocate();
        pagetable* ChildPdpt = (pagetable*)(ChildPdptPhys + gMmuVOffset);
        memset(ChildPdpt, 0, MMU_PAGE_SIZE);
        ChildPml4V[i4] = ChildPdptPhys | (Pml4Entry & 0xFFF);

        for (int i3 = 0; i3 < 512; i3++) {
            uint64_t PdptEntry = ParentPdpt[i3];
            if (!(PdptEntry & MMU_PAGE_BIT_P_PRESENT)) continue;

            pagetable* ParentPd = (pagetable*)((PdptEntry & ~0xFFFULL) + gMmuVOffset);

            uint64_t ChildPdPhys = (uint64_t)PmmAllocate();
            pagetable* ChildPd = (pagetable*)(ChildPdPhys + gMmuVOffset);
            memset(ChildPd, 0, MMU_PAGE_SIZE);
            ChildPdpt[i3] = ChildPdPhys | (PdptEntry & 0xFFF);

            for (int i2 = 0; i2 < 512; i2++) {
                uint64_t PdEntry = ParentPd[i2];
                if (!(PdEntry & MMU_PAGE_BIT_P_PRESENT)) continue;

                pagetable* ParentPt = (pagetable*)((PdEntry & ~0xFFFULL) + gMmuVOffset);

                uint64_t ChildPtPhys = (uint64_t)PmmAllocate();
                pagetable* ChildPt = (pagetable*)(ChildPtPhys + gMmuVOffset);
                memset(ChildPt, 0, MMU_PAGE_SIZE);
                ChildPd[i2] = ChildPtPhys | (PdEntry & 0xFFF);

                for (int i1 = 0; i1 < 512; i1++) {
                    uint64_t PtEntry = ParentPt[i1];
                    if (!(PtEntry & MMU_PAGE_BIT_P_PRESENT)) continue;

                    uint64_t ParentFramePhys = PtEntry & ~0xFFFULL;
                    uint64_t Flags = PtEntry & 0xFFF;

                    uint64_t ChildFramePhys = (uint64_t)PmmAllocate();
                    if (!ChildFramePhys) {
                        KATTEMPT(NULL);
                    }

                    void* ParentFrameV = (void*)(ParentFramePhys + gMmuVOffset);
                    void* ChildFrameV  = (void*)(ChildFramePhys + gMmuVOffset);
                    memcpy(ChildFrameV, ParentFrameV, MMU_PAGE_SIZE);

                    ChildPt[i1] = ChildFramePhys | Flags;
                }
            }
        }
    }

    return 0;
}

// hack for drivers cuz we cant export asm symbols (well atleast not in elf files)
uint64_t MmuGetPml4() {
    return _x86_64_get_pml4();
}
KE_EXPORT_SYMBOL(MmuGetPml4);