#include "pmm.h"
#include <printfwrapper.h>
#include "external/bootboot.h"
#include <util/spinlock.h>

#ifdef __x86_64__
#include <arch/x86_64/cpu/paging.h>
#endif

static uint64_t PmmLargestFreeMemorySize = 0;
static void* PmmLargestFreeMemoryPtr = 0x0;

static Spinlock PmmInternalLock = {ATOMIC_FLAG_INIT};
// for paging
uint64_t PmmTotalPhysicalMem = 0;

typedef struct PmmInternalBlock {
    struct PmmInternalBlock* next;
} PmmInternalBlock;
static PmmInternalBlock* PmmInternalHead;

//#define PMM_DEBUG
void PmmInitalize(BOOTBOOT* b) {
    uint64_t MMapEntries = (b->size - 128) / 16;
    MMapEnt* entry = &b->mmap;
    // so basically we find the largest free memory chunk
    for (uint64_t i = 0; i < MMapEntries; i++, entry++) {
        if (MMapEnt_IsFree(entry)) {
            if (MMapEnt_Size(entry) > PmmLargestFreeMemorySize) {
                // found one
                PmmLargestFreeMemorySize = MMapEnt_Size(entry);
                PmmLargestFreeMemoryPtr = (void*)MMapEnt_Ptr(entry);
                #ifdef PMM_DEBUG
                printf("pmm: dbg: found new largest free memory chunk. size=%d ptr=0x%lx\r\n", PmmLargestFreeMemorySize, PmmLargestFreeMemoryPtr);
                #endif
            }
        }
        PmmTotalPhysicalMem += MMapEnt_Size(entry);
    }
    PmmInternalHead = PmmLargestFreeMemoryPtr;
    uint64_t PmmLargestFreeMemoryPages = PmmLargestFreeMemorySize / PAGE_SIZE;

    // build the list
    for (uint64_t i = 0; i < PmmLargestFreeMemoryPages; i++) {
        PmmFree((void*)((uint64_t)PmmLargestFreeMemoryPtr + (i * PAGE_SIZE)));
    }
}

void* PmmAllocate() {
    SpnLckAcquire(&PmmInternalLock);
    PmmInternalBlock* tmp = PmmInternalHead;
    PmmInternalHead = PmmInternalHead->next;
    // check for out of memory
    if ((uint64_t)PmmInternalHead > (uint64_t)(PmmLargestFreeMemoryPtr + PmmLargestFreeMemorySize)) {
        printf("pmm: ran out of physical memory!\r\n");
        SpnLckRelease(&PmmInternalLock);
        return NULL;
    }
    SpnLckRelease(&PmmInternalLock);
    return tmp;
}

void* PmmAllocatePages(int pages) {
    SpnLckAcquire(&PmmInternalLock);
    
}
void PmmFree(void *page) {
    SpnLckAcquire(&PmmInternalLock);
    PmmInternalBlock* tmp = page;
    tmp->next = PmmInternalHead;
    PmmInternalHead = tmp;
    SpnLckRelease(&PmmInternalLock);
}