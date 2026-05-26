#pragma once
#include <stdint.h>

#define PAGE_SIZE 4096

#define MMU_PAGE_SIZE 4096
#define MMU_PAGE_SIZE2MIB 0x200000
#define MMU_PAGE_BIT_P_PRESENT (1<<0)
#define MMU_PAGE_BIT_RW_WRITABLE (1<<1)
#define MMU_PAGE_BIT_US_USER (1<<2)
#define MMU_PAGE_BIT_PWT (1<<3)
#define MMU_PAGE_BIT_PCD (1<<4)

#define MMU_PAGE_XD_NX (1<<63)

#define MMU_PAGE_BIT_PS (1<<7)

#define MMU_PAGE_ADDR_MASK 0x000ffffffffff000

#define MMU_PAGE_BIT_A_ACCESSED (1<<5)
#define MMU_PAGE_BIT_D_DIRTY (1<<6)

// virtual address of a phys address = (MMU_PHYS_OFFSET + physaddr)
#define MMU_PHYS_OFFSET 0xffff888000000000

typedef uint64_t physaddr;
typedef uint64_t virtaddr;

typedef uint64_t pagetable __attribute__((aligned(MMU_PAGE_SIZE)));

void MmuMapPage(pagetable* pml4, virtaddr virt, physaddr phys, unsigned int flags);
void MmuMapRegion(pagetable* pml4, virtaddr vstart, physaddr pstart, physaddr pend, unsigned int flags);
physaddr MmuGetPhys(virtaddr virt);
void MmuUnmapPage(pagetable* pml4, virtaddr virt);
//void MapLargePage(physaddr physical, virtaddr virtual, unsigned int flags) ;

extern uint64_t gMmuVOffset;
extern void _x86_64_load_pml4(uint64_t);
extern uint64_t _x86_64_get_pml4();