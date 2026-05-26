
#include "kernel.h"
#include "serial.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __x86_64__
#include <arch/x86_64/cpu/paging.h>
#endif
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <memory.h>
#include <util/spinlock.h>

typedef struct VmmInternalBlock {
    uint64_t start;
    uint64_t size;
    uint8_t free;
    struct VmmInternalBlock* prev;
    struct VmmInternalBlock* next;
} VmmInternalBlock;

static VmmInternalBlock* VmmInternalHead = NULL;
static Spinlock VmmInternalLock = {ATOMIC_FLAG_INIT};

//static KernelInformation* gkInfo;

// pool of vmminternalblocks so we dont call pmmallocate everytime
static VmmInternalBlock* VmmBlockPool = NULL;
static uint32_t VmmBlockPoolIdx = 0;

static VmmInternalBlock* VmmFreeBlockHead = NULL;
VmmInternalBlock* VmmInternalAllocBlock() {
    // check free blocks first
    if (VmmFreeBlockHead != NULL) {
        VmmInternalBlock* tmp = VmmFreeBlockHead;
        VmmFreeBlockHead = VmmFreeBlockHead->next;
        memset(tmp, 0, sizeof(VmmInternalBlock));
        return tmp;
    }
    if (VmmBlockPool == NULL || VmmBlockPoolIdx >= (MMU_PAGE_SIZE) / sizeof(VmmInternalBlock)) {
        VmmBlockPool = (VmmInternalBlock*)(PmmAllocate() + gMmuVOffset);
        VmmBlockPoolIdx = 0;
        memset(VmmBlockPool, 0, MMU_PAGE_SIZE);
    }
    return &VmmBlockPool[VmmBlockPoolIdx++];
}

void VmmInitalize() {
    VmmInternalHead = VmmInternalAllocBlock();
    if (!VmmInternalHead) {
        WriteSerial(0x3f8, "vmm: panic\r\n");
        return;
    }

    VmmInternalHead->start = ((uint64_t)MMU_PHYS_OFFSET + 0x40000000); // 1GB after HHDM
    VmmInternalHead->size = 0x100000000; // 4GB
    
    VmmInternalHead->free = 1;
    VmmInternalHead->next = NULL;
    VmmInternalHead->prev = NULL;
}

VmmInternalBlock* VmmInternalFindGap(uint64_t size) {
    VmmInternalBlock* current = VmmInternalHead;
    while (current != NULL) {
        if ((current->free == 1) && current->size >= size) {
            return current;
        }
        current->next->prev = current;
        current = current->next;
    }
    return NULL; // didn't find any :(
}

VmmInternalBlock* VmmInternalFindAddress(void* ptr) {
    // find which block contains ptr
    VmmInternalBlock* current = VmmInternalHead;
    while (current != NULL) {
        if (current->start == (uint64_t)ptr) {
            return current;
        }
        current->next->prev = current;
        current = current->next;
    }
    return NULL; // couldn't find
}

void* VmmAllocate(uint64_t size) {
    SpnLckAcquire(&VmmInternalLock);
    VmmInternalBlock* block = VmmInternalFindGap(size);
    if (!block) { SpnLckRelease(&VmmInternalLock); return NULL; } 
    if (block->size > size) { // if the block we get is larger than what we wanted
        VmmInternalBlock* new = VmmInternalAllocBlock();
        // new node
        new->start = block->start + size;
        new->size = block->size - size;
        new->free = 1;
        new->next = block->next;
        new->prev = block;

        if (block->next) {
            block->next->prev = new;
        }
        // update old node
        block->size = size;
        block->free = 0;
        block->next = new;
    }
    block->free = 0;
    // map it
    for (uint64_t i = 0; i < size; i+=MMU_PAGE_SIZE) {
        physaddr f = (physaddr)PmmAllocate();
        MmuMapPage((pagetable*)_x86_64_get_pml4(), i + block->start, f, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
        memset((void*)(i + block->start), 0, MMU_PAGE_SIZE);
    }
    SpnLckRelease(&VmmInternalLock);
    return (void*)block->start;
}

void VmmFree(void* ptr) {
    SpnLckAcquire(&VmmInternalLock);
    // find block with ptr
    VmmInternalBlock* block = VmmInternalFindAddress(ptr);
    if (!block)  { SpnLckRelease(&VmmInternalLock); return; }
    if (block->free == 1) { SpnLckRelease(&VmmInternalLock); return; }
    // unmap addresses
    for (uint64_t i = block->start; i < (block->start + block->size); i+=MMU_PAGE_SIZE) {
        MmuUnmapPage((pagetable*)_x86_64_get_pml4(), i);
    }
    // mark as free
    block->free = 1;
    // merge blocks (forward)
    if (block->next != NULL && block->next->free == 1) {
        // merge into current
        block->size += block->next->size;
        VmmInternalBlock* deadNode = block->next;
        block->next = deadNode->next;
        if (block->next != NULL) {
            block->next->prev = block;
        }
        deadNode->next = VmmFreeBlockHead;
        VmmFreeBlockHead = deadNode;
    }
    // merge blocks backward
    if (block->prev != NULL && block->prev->free == 1) {
        block->prev->size += block->size;
        VmmInternalBlock* deadNode = block;
        block->prev->next = block->next;

        if (block->next != NULL) {
            block->next->prev = block->prev;
        }
        deadNode->next = VmmFreeBlockHead;
        VmmFreeBlockHead = deadNode;
    }
    SpnLckRelease(&VmmInternalLock);
}