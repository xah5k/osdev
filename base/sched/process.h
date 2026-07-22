#pragma  once
#ifdef __x86_64__
#include <arch/x86_64/cpu/paging.h>
#endif
#include <kernel.h>
#include <fs/vfs.h>

#define PS_USER_STACK_PAGES 4
#define PS_USER_STACK_BASE 0x00007FFFFFFFF000


#define SCHED_THREAD_READY 1
#define SCHED_THREAD_RUNNING 2
#define SCHED_THREAD_DEAD 3


#define SCHED_PRIV_KERNEL 0
#define SCHED_PRIV_USER 1


typedef struct ProcessCtrlBlk {
    physaddr cr3;
    virtaddr* pml4;
    uint64_t pid;
    int nextfh;
    int threads;
    VfsOpenFileDescr* FileHandleTable;
    struct ThreadCtrlBlk* ThreadListHead;
    struct ProcessCtrlBlk* Next;
} ProcessCtrlBlk;

typedef struct ThreadCtrlBlk {
#ifdef __x86_64__
    uint64_t KernelRsp;
    uint64_t KernelStackBase; // vaddr
    uint64_t UserRsp;
    uint64_t UserStackBase; // vaddr
    uint32_t tid;
    uint8_t state;
    uint8_t priority;
    void* entry;
    uint8_t privilege; // 0 = kernel, 1 = user
    uint8_t exitcode;
    uint8_t pendingkill;
    struct ProcessCtrlBlk* ParentProc;
    struct ThreadCtrlBlk* GlobalNext; // next thread in the actual global list of threads (scheduler doesnt care about which process it belongs to)
    struct ThreadCtrlBlk* ProcNext; // next thread that shares the same process
#endif
} ThreadCtrlBlk;

ProcessCtrlBlk* ProcFindByPid(uint64_t pid, KernelInformation* kinfo);
ThreadCtrlBlk* ThreadNew(void* entry, uint8_t priv);
ProcessCtrlBlk* ProcessNew();
void ProcessCreate(void* entry, KernelInformation* kinfo, uint8_t priv);
void ProcessCreate2(void* entry, KernelInformation* kinfo, void* arg1);
void ThreadMapUserStack(ThreadCtrlBlk* Tcb);
uint64_t* ProcNewPML4();
void ThreadEntry();
void ProcListRunning(KernelInformation* kinfo);
void ThreadAdd(ThreadCtrlBlk* Tcb);
void ProcAttachThread(ProcessCtrlBlk* proc, ThreadCtrlBlk* tcb);
void ProcFreePML4(pagetable* pml4p);
void ThrCheckPendingKill();