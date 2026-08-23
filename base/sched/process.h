#pragma  once
#include <hal/mmu.h>
#include <kernel.h>
#include <fs/vfs.h>
#include <ttyobj.h>
#include <sched/ipc/signal.h>

#define PS_USER_STACK_PAGES 4
#define PS_USER_STACK_BASE 0x00007FFFFFFFF000

#define PS_USER_BRK_BASE 0x700000000000
#define PS_USER_BRK_SIZE 0x40000000

// only if hint=0x0 otherwise js use the hint
#define PS_USER_MMAPDEC_BASE 0x0000700000000000ULL

#define SCHED_THREAD_READY 1
#define SCHED_THREAD_RUNNING 2
#define SCHED_THREAD_DEAD 3
#define SCHED_THREAD_SUSPENDED 4

#define SCHED_PRIV_KERNEL 0
#define SCHED_PRIV_USER 1

typedef struct MmapEntry {
    virtaddr Vaddr;
    uint64_t Length;
    int Prot;
    int Flags;
    struct MmapEntry* Next;
} MmapEntry;

typedef struct ProcessCtrlBlk {
    char name[256]; // after like 20 years
    char cwd[VFS_MAX_ALLOWED_PATH];
    physaddr cr3;
    virtaddr* pml4;
    uint64_t pid;
    int nextfh;
    int threads;
    uint64_t SbrkBase;
    uint64_t SbrkCurrent;
    uint64_t SbrkLimit;
    uint64_t exitcode;
    uint64_t Mode; // stub for umask
    KeTerminalObj* TtyObj;
    MmapEntry* MmapEntryHead;
    uint64_t MmapBumpNext;
    KeSignalHdlObj Handlers[KE_SIGLIST_MAX];
    struct ThreadCtrlBlk* BlockedQueueHead;
    struct ThreadCtrlBlk* BlockedQueueTail;
    VfsOpenFileDescr* FileHandleTable;
    struct ThreadCtrlBlk* ThreadListHead;
    struct ProcessCtrlBlk* Parent;
    struct ProcessCtrlBlk* Next;
} ProcessCtrlBlk;

typedef struct ThreadCtrlBlk {
    uint64_t KernelRsp;
    uint64_t KernelStackBase; // vaddr
    uint64_t UserRsp;
    uint64_t UserStackBase; // vaddr
    uint32_t tid;
    uint8_t state;
    uint8_t priority;
    void* entry;
    uint8_t privilege; // 0 = kernel, 1 = user
    uint64_t exitcode;
    uint64_t SigPendingSet;
    uint64_t SigBlockedSet;
    char** UserArgv;
    int UserArgc;
    #ifdef __x86_64__
    uint64_t FsBase;
    uint64_t GsBase;
    #else
    uint64_t Rsv0;
    uint64_t Rsv1;
    #endif
    struct ProcessCtrlBlk* ParentProc;
    struct ThreadCtrlBlk* GlobalNext; // next thread in the actual global list of threads (scheduler doesnt care about which process it belongs to)
    struct ThreadCtrlBlk* ProcNext; // next thread that shares the same process
    CpuInterruptArgs* LastIframe;
} ThreadCtrlBlk;

ThreadCtrlBlk* ThrGetCurrent();
ProcessCtrlBlk* ProcFindByPid(uint64_t pid, KernelInformation* kinfo);
ThreadCtrlBlk* ThreadNew(void* entry, uint8_t priv, const char** argv, int argc, const char** envp, int envc);
ProcessCtrlBlk* ProcessNew(char* name);
void ProcessCreate(void* entry, KernelInformation* kinfo, uint8_t priv);
uint64_t ProcessCopy(ProcessCtrlBlk* proc, ThreadCtrlBlk* caller, CpuInterruptArgs* frame);
void ThreadPushTail(ThreadCtrlBlk** Head, ThreadCtrlBlk** Tail, ThreadCtrlBlk* Tcb);
ThreadCtrlBlk* ThreadPopHead(ThreadCtrlBlk** Head, ThreadCtrlBlk** Tail);
void ThreadWake(ThreadCtrlBlk* Tcb);
void ThreadEntry();
void ProcListRunning(KernelInformation* kinfo);
void ThreadAdd(ThreadCtrlBlk* Tcb);
void ProcAttachThread(ProcessCtrlBlk* proc, ThreadCtrlBlk* tcb);
void ThrCheckSignals(CpuInterruptArgs* OldCtx);