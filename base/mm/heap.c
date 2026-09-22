#include <mm/vmm.h>
#include <mm/heap.h>
#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <printfwrapper.h>
#include <util/spinlock.h>
#include <kedriver.h>

#define MM_HEAP_SIZE 8192000*2 // 16MB
#define MM_HEAP_MAGIC 0xDEFDEF
#define MM_HEAP_FREE 0
#define MM_HEAP_USED 1

typedef struct _MmHeapHeader {
    uint32_t Magic;
    uint64_t Size;
    uint16_t Status;
    uint16_t Unused;
    struct _MmHeapHeader* Next;
} MmHeapBlockHdr;

static MmHeapBlockHdr* BlockListHead;
static MmHeapBlockHdr* PreviousBlock;

static Spinlock MmInternalHeapLock = {ATOMIC_FLAG_INIT};

KSTATUS MmHeapInitalize() {
    MmHeapBlockHdr* Start;
    Start = VmmAllocate(MM_HEAP_SIZE);
    Start->Size = MM_HEAP_SIZE;
    Start->Status = MM_HEAP_FREE;
    Start->Next = NULL;
    Start->Magic = MM_HEAP_MAGIC;
    BlockListHead = Start;
    PreviousBlock = NULL;
    return KSUCCESS;
}

// dumps it out to printf
void MmHeapDumpMap() {
    uint64_t r = SpnLckAcquireRfl(&MmInternalHeapLock);
    MmHeapBlockHdr* Current = BlockListHead;
    int i = 0;
    while (Current != NULL) {
        printf("kheap: Block #%d: addr: @0x%lx size=%d status=%d\r\n", i, ((uint64_t)Current + sizeof(MmHeapBlockHdr)), Current->Size, Current->Status);
        i++;
        Current = Current->Next;
    }
    SpnLckReleaseRfl(&MmInternalHeapLock, r);
}

MmHeapBlockHdr* MmInternalFindBlSz(uint64_t size) {
    MmHeapBlockHdr* Current = BlockListHead;
    while (Current != NULL) {
        if (Current->Status == MM_HEAP_FREE) {
            if (Current->Size >= size) {
                return Current;
            }   
        }
        PreviousBlock = Current;
        Current = Current->Next;
    }
    return NULL;
}

void* MmAllocate(uint64_t size) {
    void* Result = NULL;
    uint64_t r = SpnLckAcquireRfl(&MmInternalHeapLock);
    uint64_t TotalSize = ((size + 15) & ~15) + sizeof(MmHeapBlockHdr);
    MmHeapBlockHdr* Block = MmInternalFindBlSz(TotalSize);

    if (!Block) {
        Result = NULL; 
        goto end;
    }
    if (Block->Size < (TotalSize + sizeof(MmHeapBlockHdr) + 16))  { 
        if (Block == BlockListHead) {
            BlockListHead = Block->Next;
        } else {
            PreviousBlock->Next = Block->Next;
        }
        Block->Status = MM_HEAP_USED;
        Block->Magic = MM_HEAP_MAGIC;
        Result = (void*)(Block + 1); 
    } else {
        //uint64_t BfSize = Block->Size;
        Block->Size -= TotalSize;
        
        MmHeapBlockHdr* NewUsedBlock = (MmHeapBlockHdr*)((uintptr_t)Block + Block->Size);
        NewUsedBlock->Size = TotalSize;
        NewUsedBlock->Status = MM_HEAP_USED;
        NewUsedBlock->Magic = MM_HEAP_MAGIC;
        Result = (void*)(NewUsedBlock + 1);
    }
    end:
    SpnLckReleaseRfl(&MmInternalHeapLock, r);
    return Result;
}
KE_EXPORT_SYMBOL(MmAllocate);
void MmFree(void* ptr) {
    uint64_t r = SpnLckAcquireRfl(&MmInternalHeapLock);
    if (!ptr) { SpnLckReleaseRfl(&MmInternalHeapLock, r); return; }
    MmHeapBlockHdr* Block = (MmHeapBlockHdr*)ptr - 1;
    Block->Status = MM_HEAP_FREE;

    if (BlockListHead == NULL || (uintptr_t)Block < (uintptr_t)BlockListHead) {
        Block->Next = BlockListHead;
        BlockListHead = Block;
        
        if (Block->Next != NULL && (uintptr_t)Block + Block->Size == (uintptr_t)Block->Next) {
            Block->Size += Block->Next->Size;
            Block->Next = Block->Next->Next;
        }
    } else {
        MmHeapBlockHdr* Current = BlockListHead;
        while (Current->Next != NULL && (uintptr_t)Current->Next < (uintptr_t)Block) {
            Current = Current->Next;
        }

        Block->Next = Current->Next;
        Current->Next = Block;

        if (Block->Next != NULL && (uintptr_t)Block + Block->Size == (uintptr_t)Block->Next) {
            Block->Size += Block->Next->Size;
            Block->Next = Block->Next->Next;
        }

        if ((uintptr_t)Current + Current->Size == (uintptr_t)Block) {
            Current->Size += Block->Size;
            Current->Next = Block->Next;
        }
    }
    SpnLckReleaseRfl(&MmInternalHeapLock, r);
}
KE_EXPORT_SYMBOL(MmFree);