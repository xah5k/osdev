#include "paging.h"
#include <mm/pmm.h>
#include <memory.h>
#include <stdint.h>

// offset used for memory
uint64_t gMmuVOffset = 0;

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

physaddr MmuGetPhys(virtaddr virt) {
    uint64_t cr3 = _x86_64_get_pml4();
    
    uint64_t* pml4 = (uint64_t*)((cr3 & ~0xFFF) + gMmuVOffset);
    uint64_t pml4e = pml4[(virt >> 39) & 0x1FF];
    if (!(pml4e & 1)) return 0;

    uint64_t* pdpt = (uint64_t*)((pml4e & ~0xFFF) + gMmuVOffset);
    uint64_t pdpte = pdpt[(virt >> 30) & 0x1FF];
    if (!(pdpte & 1)) return 0;
    if (pdpte & 0x80) return (pdpte & ~0x3FFFFFFF) + (virt & 0x3FFFFFFF); 

    uint64_t* pd = (uint64_t*)((pdpte & ~0xFFF) + gMmuVOffset);
    uint64_t pde = pd[(virt >> 21) & 0x1FF];
    if (!(pde & 1)) return 0;
    if (pde & 0x80) return (pde & ~0x1FFFFF) + (virt & 0x1FFFFF); 

    uint64_t* pt = (uint64_t*)((pde & ~0xFFF) + gMmuVOffset);
    uint64_t pte = pt[(virt >> 12) & 0x1FF];
    if (!(pte & 1)) return 0;

    return (pte & ~0xFFF) + (virt & 0xFFF);
}

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

void MmuMapRegion(pagetable* pml4, virtaddr vstart, physaddr pstart, physaddr pend, unsigned int flags) {
    vstart &= ~0xFFFULL;
    pstart &= ~0xFFFULL;
    pend = (pend + 0xFFFULL) & ~0xFFFULL;
    for (uint64_t i = 0; i < (pend - pstart); i+=MMU_PAGE_SIZE) {
        MmuMapPage(pml4, vstart + i, pstart + i, flags);
    }
}