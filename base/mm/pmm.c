#include "pmm.h"
#include <printfwrapper.h>
#include "external/bootboot.h"
#include <util/spinlock.h>

#ifdef __x86_64__
#include <arch/x86_64/cpu/paging.h>
#endif
#include <kernel.h>
#include <memory.h>
#include <kedriver.h>

static uint64_t PmmLargestFreeMemorySize = 0;
static void* PmmLargestFreeMemoryPtr = 0x0;

static Spinlock PmmInternalLock = {ATOMIC_FLAG_INIT};
// for paging
uint64_t PmmTotalPhysicalMem = 0;
uint64_t PmmTotalFreePhysRam = 0;
static uint64_t* PmmInternalBitmap;
static uint64_t PmmInternalBitmapSz = 0;
static uint64_t PmmTotalPages = 0;
static uint64_t PmmHighestAddr = 0x0;
//#define PMM_DEBUG

KSTATUS PmmInitalize(BOOTBOOT* b) {
    uint64_t MMapEntries = (b->size - 128) / 16;
    MMapEnt* entry = &b->mmap;
    uint64_t TopAddress = 0;
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
            PmmTotalFreePhysRam += MMapEnt_Size(entry);
        }
        TopAddress = MMapEnt_Ptr(entry) + MMapEnt_Size(entry);
        if (TopAddress > PmmHighestAddr) {
            PmmHighestAddr = TopAddress;
        }
        PmmTotalPhysicalMem += MMapEnt_Size(entry);
    }
    uint64_t BitmapSize = ((PmmHighestAddr / PAGE_SIZE) + 7) / 8;

    if (BitmapSize > PmmLargestFreeMemorySize) {
        return KOOMERR;
    }
    PmmInternalBitmap = PmmLargestFreeMemoryPtr;
    memset((void*)PmmInternalBitmap, 0xFF, BitmapSize);

    entry = &b->mmap;
    for (uint64_t x = 0; x < MMapEntries; x++, entry++) {
        if (MMapEnt_IsFree(entry)) {
            uint64_t BasePage = MMapEnt_Ptr(entry) / PAGE_SIZE;
            for (uint64_t i = BasePage; i < (BasePage + (MMapEnt_Size(entry) / PAGE_SIZE)); i++) {
                uint64_t ArrayIdx = i >> 6;
                uint64_t BitPos =  i & 63;
                PmmInternalBitmap[ArrayIdx] &= ~(1ULL << BitPos);
            }
        }
    }
    uint64_t BitmapStartPage = (uint64_t)PmmInternalBitmap / PAGE_SIZE;
    for (uint64_t i = BitmapStartPage; i < (BitmapStartPage + (BitmapSize / PAGE_SIZE)); i++) {
        uint64_t ArrayIdx = i >> 6;
        uint64_t BitPos =  i & 63;
        PmmInternalBitmap[ArrayIdx] |= (1ULL << BitPos);
    }
    PmmInternalBitmapSz = BitmapSize;
    PmmTotalPages = (PmmLargestFreeMemorySize) / PAGE_SIZE;
    PmmInternalBitmap[0] |= 1ULL;
    return KSUCCESS;
}

void PmmAdjustBitmapPtr() {
    PmmInternalBitmap = (uint64_t*)((uint64_t)PmmInternalBitmap + gMmuVOffset);
    
}

void* PmmAllocate() {
    return PmmAllocatePages(1);
}
KE_EXPORT_SYMBOL(PmmAllocate);
void* PmmAllocatePages(uint64_t num) {
    SpnLckAcquire(&PmmInternalLock);
    uint64_t TotalBitmapEntries = PmmInternalBitmapSz/8;
    uint64_t AllocatedPages = 0;
    uint64_t PotentialStartPageIdx = 0;
    for (uint64_t i = 0; i < TotalBitmapEntries; i++) {
        if (PmmInternalBitmap[i] == UINT64_MAX) {
            AllocatedPages = 0;
            continue;
        }
        for (int j = 0; j < 64; j++) {
            uint64_t IsAlreadyAlloc = PmmInternalBitmap[i] & (1ULL << j);
            uint64_t GlobalPageIdx = (i * 64) + j;
            if (IsAlreadyAlloc == 0) {
                if (AllocatedPages == 0) PotentialStartPageIdx = GlobalPageIdx;
                AllocatedPages += 1;
                if (AllocatedPages == num) goto out;
            } else AllocatedPages = 0;
        }
    }
    SpnLckRelease(&PmmInternalLock);
    return NULL;
    out:
    for (uint64_t i = PotentialStartPageIdx; i < (PotentialStartPageIdx + num); i++) {
        uint64_t ArrayIdx = i >> 6;
        uint64_t BitPos =  i & 63;
        PmmInternalBitmap[ArrayIdx] |= (1ULL << BitPos);
    }
    SpnLckRelease(&PmmInternalLock);
    return (void*)(PotentialStartPageIdx * PAGE_SIZE);
}
KE_EXPORT_SYMBOL(PmmAllocatePages);
void PmmFree(void *page) {
    PmmFreePages(page, 1);
}
KE_EXPORT_SYMBOL(PmmFree);
void PmmFreePages(void* pagef, uint64_t num) {
    if (!pagef) return;
    SpnLckAcquire(&PmmInternalLock);
    uint64_t Idx = ((uint64_t)pagef / 4096);
    if ((Idx + num) > (PmmInternalBitmapSz * 8)) {
        SpnLckRelease(&PmmInternalLock);
        return;
    }
    for (uint64_t i = Idx; i < Idx + num; i++) {
        uint64_t ArrayIdx = i >> 6;
        uint64_t BitPos = i & 63;
        PmmInternalBitmap[ArrayIdx] &= ~(1ULL << BitPos);
    }
    SpnLckRelease(&PmmInternalLock);
}
KE_EXPORT_SYMBOL(PmmFreePages);