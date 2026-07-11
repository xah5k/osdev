#pragma  once
#ifdef __x86_64__
#include <arch/x86_64/cpu/paging.h>
#endif
#include <kernel.h>
#include <fs/vfs.h>
typedef struct ProcessCtrlBlk {
    physaddr cr3;
    virtaddr* pml4;
    uint64_t pid;
    int nextfh;
    int threads;
    VfsOpenFileDescr* FileHandleTable[VFS_MAX_ALLOWED_OPEN_HANDLES];
    struct ThreadCtrlBlk* ThreadListHead;
    struct ProcessCtrlBlk* Next;
} ProcessCtrlBlk;

typedef struct ThreadCtrlBlk {
#ifdef __x86_64__
    uint64_t rsp;
    uint64_t StackBase;
    uint32_t tid;
    uint8_t state;
    uint8_t priority;
    void* entry;
    struct ProcessCtrlBlk* ParentProc;
    struct ThreadCtrlBlk* GlobalNext; // next thread in the actual global list of threads (scheduler doesnt care about which process it belongs to)
    struct ThreadCtrlBlk* ProcNext; // next thread that shares the same process
#endif
} ThreadCtrlBlk;


ThreadCtrlBlk* ThreadNew(void* entry);
ProcessCtrlBlk* ProcessNew();
void ProcessCreate(void* entry, KernelInformation* kinfo);
uint64_t* ProcNewPML4();
void ThreadEntry();
void ProcListRunning(KernelInformation* kinfo);
void ThreadAdd(ThreadCtrlBlk* Tcb);
void ProcAttachThread(ProcessCtrlBlk* proc, ThreadCtrlBlk* tcb);