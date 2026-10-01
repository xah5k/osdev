#pragma  once
#include <hal/mmu.h>
#include <kernel.h>
#include <fs/vfs.h>
#include <ttyobj.h>
#include <sched/ipc/signal.h>
#include <sched/ipc/msg.h>
#include <util/spinlock.h>
struct KeSchedQueue;

#define PS_USER_STACK_PAGES 64
#define PS_USER_STACK_BASE 0x00007FFFFFFFF000

#define PS_USER_BRK_BASE 0x700000000000
#define PS_USER_BRK_SIZE 0x40000000

// only if hint=0x0 otherwise js use the hint
#define PS_USER_MMAPDEC_BASE 0x0000700000000000ULL

#define SCHED_THREAD_READY 1
#define SCHED_THREAD_RUNNING 2
#define SCHED_THREAD_DYING 3
#define SCHED_THREAD_DEAD 4
#define SCHED_THREAD_SUSPENDED 5

#define SCHED_PRIV_KERNEL 0
#define SCHED_PRIV_USER 1

#define SCHED_THREAD_PLOW 0
#define SCHED_THREAD_PMEDIUM 16
#define SCHED_THREAD_PHIGH 32

typedef struct MmapEntry {
    virtaddr Vaddr;
    uint64_t Length;
    int Prot;
    int Flags;
    struct MmapEntry* Next;
} MmapEntry;

typedef struct ProcessCtrlBlk {
    char Name[256]; // after like 20 years
    char Cwd[VFS_MAX_ALLOWED_PATH];
    physaddr cr3;
    virtaddr* pml4;
    uint64_t Pid;
    int NextFh;
    int Threads;
    uint64_t SbrkBase;
    uint64_t SbrkCurrent;
    uint64_t SbrkLimit;
    uint64_t Exitcode;
    uint64_t Mode; // stub for umask
    KeTerminalObj* TtyObj;
    MmapEntry* MmapEntryHead;
    uint64_t MmapBumpNext;
    KeSignalHdlObj Handlers[KE_SIGLIST_MAX];
    struct ThreadCtrlBlk* BlockedQueueHead;
    struct ThreadCtrlBlk* BlockedQueueTail;
    VfsOpenFileDescr* FileHandleTable;
    KeMessageObj* MessageHead;
    KeMessageObj* MessageTail;
    Spinlock MessageQueueLock;
    Spinlock MmapListLock;
    uint64_t MessageCount;
    struct KeSchedQueue* ThreadList;
    struct ProcessCtrlBlk* Parent;
    struct ProcessCtrlBlk* Next;
} ProcessCtrlBlk;

typedef struct ThreadCtrlBlk {
    uint64_t KernelRsp;
    uint64_t KernelStackBase; // vaddr
    uint64_t UserRsp;
    uint64_t UserStackBase; // vaddr
    uint32_t Tid;
    uint8_t State;
    uint32_t Priority;
    uint32_t Bpriority; // priority when first created
    uint32_t Deadline; // ticks countdown
    uint32_t TickDefault;
    void* entry;
    uint8_t Privilege; // 0 = kernel, 1 = user
    uint64_t Exitcode;
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
    struct ThreadCtrlBlk* GlobalPrev;
    struct ThreadCtrlBlk* ProcNext; // next thread that shares the same process
    struct ThreadCtrlBlk* ProcPrev;
    struct ThreadCtrlBlk* DeathNext;
    struct ThreadCtrlBlk* DeathPrev;
    CpuInterruptArgs* LastIframe;
    uint64_t CpuNum;
} ThreadCtrlBlk;

// dumb macros prob should make a function instead
#define PROC_THRADD(Queue, Tcb) do { \
    Tcb->ProcNext = NULL; \
    Tcb->ProcPrev = Queue->Tail; \
    if (Queue->Tail) Queue->Tail->ProcNext = Tcb; \
    else Queue->Head = Tcb; \
    Queue->Tail = Tcb; \
} while(0); \

#define PROC_THRRM(Queue, Tcb) do { \
    if (Tcb->ProcPrev) Tcb->ProcPrev->ProcNext = Tcb->ProcNext; \
    else Queue->Head = Tcb->ProcNext; \
    if (Tcb->ProcNext) Tcb->ProcNext->ProcPrev = Tcb->ProcPrev; \
    else Queue->Tail = Tcb->ProcPrev; \
} while (0); \

#define THR_DEATHADD(Queue, Tcb) do { \
    Tcb->DeathNext = NULL; \
    Tcb->DeathPrev = Queue->Tail; \
    if (Queue->Tail) Queue->Tail->DeathNext = Tcb; \
    else Queue->Head = Tcb; \
    Queue->Tail = Tcb; \
} while(0); \

#define THR_DEATHRM(Queue, Tcb) do { \
    if (Tcb->DeathPrev) Tcb->DeathPrev->DeathNext = Tcb->DeathNext; \
    else Queue->Head = Tcb->DeathNext; \
    if (Tcb->DeathNext) Tcb->DeathNext->DeathPrev = Tcb->DeathPrev; \
    else Queue->Tail = Tcb->DeathPrev; \
} while (0); \

void ThrDeathMark(ThreadCtrlBlk* Tcb);
void ThrDeathCleanup();
void ThreadRemove(ThreadCtrlBlk* Tcb);
ThreadCtrlBlk* ThreadNext(struct KeScheduler* Sched);
ThreadCtrlBlk* ThrGetCurrent();
ProcessCtrlBlk* ProcFindByPid(uint64_t pid, KernelInformation* kinfo);
ThreadCtrlBlk* ThreadNew(void* entry, uint8_t priv, uint32_t prior, const char** argv, int argc, const char** envp, int envc);
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
void ThreadQueueAdd(struct KeSchedQueue* Queue, ThreadCtrlBlk* Tcb);
void ThreadQueueRemove(struct KeSchedQueue* Queue, ThreadCtrlBlk* Tcb);