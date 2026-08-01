#include <ksyscall.h>
#ifdef __x86_64__
#include <arch/x86_64/archsyscall.h>
#include <arch/x86_64/cpu/lapic.h>
#endif
#include <sched/process.h>
#include <sched/sched.h>
#include <util/util.h>
#include "util/spinlock.h"
#include <printfwrapper.h>
#include <mm/heap.h>
#include <exeldr/ldrelf.h>
#include <mm/pmm.h>
#include <memory.h>
#include <abi-bits/seek.h>
#include <external/posix/stat.h>
#include <external/posix/dirent.h>
#include <external/utsname.h>
extern Spinlock SchedSpinlock;

extern ThreadCtrlBlk* CurrentThread;
extern ThreadCtrlBlk* ReadyQueueHead;
extern ThreadCtrlBlk* DeathThread;
static void UserAcBegin() {
    if (KernelGetInformation()->cpufeats->smap) {
        asm volatile ("stac");
    }
}
static void UserAcEnd() {
    if (KernelGetInformation()->cpufeats->smap) {
        asm volatile ("clac");
    }
}
uint64_t SysExit(uint64_t exitcode, KE_SYSCALL_ARGS_UNUSED1) {
    printf("ksyscall: SysExit: exit current thread with code %d\r\n", exitcode);
    // handle exit
    CurrentThread->exitcode = exitcode;
    CurrentThread->ParentProc->exitcode = exitcode;
    // wake up anyone waiting
    ThreadCtrlBlk* blthr = ThreadPopHead(&CurrentThread->ParentProc->BlockedQueueHead, &CurrentThread->ParentProc->BlockedQueueTail);
    while (blthr != NULL) {
        ThreadWake(blthr);
        blthr = ThreadPopHead(&CurrentThread->ParentProc->BlockedQueueHead, &CurrentThread->ParentProc->BlockedQueueTail); // threadpophead nulls out globalnext
    }
    SpnLckAcquire(&SchedSpinlock);
    //printf("process: handling exit of thread(tid=%d, belonging to pid %d)\r\n", CurrentThread->tid, CurrentThread->ParentProc->pid);
    ThreadCtrlBlk* c = CurrentThread;

    // remove it from process list of threads

    if (c == c->ParentProc->ThreadListHead) {
        c->ParentProc->ThreadListHead = c->ParentProc->ThreadListHead->ProcNext;
        c->ParentProc->threads--;
        goto _2;
    }
    ThreadCtrlBlk* current1 = c->ParentProc->ThreadListHead;
    ThreadCtrlBlk* previous1 = NULL;
    while (current1 != NULL) {
        if (current1 == c) {
            break;
        }
        previous1 = current1;
        current1 = current1->ProcNext;
    }

    KATTEMPT(current1);
    KATTEMPT(current1 == c);
    // unlink from list
    previous1->ProcNext = current1->ProcNext;
    _2:
    if (c != CurrentThread) {
        if (c == ReadyQueueHead) {
            ReadyQueueHead = c->GlobalNext;
            c->GlobalNext = NULL;
            goto _3;
        }
        
        ThreadCtrlBlk* current2 = ReadyQueueHead;
        ThreadCtrlBlk* previous2 = NULL;
        while (current2 != NULL && current2 != c) {
            //printf("current2=0x%lx\r\n", current2);
            previous2 = current2;
            current2 = current2->GlobalNext;
        }

        if (!current2 || current2 != c) {
           // printf("current2=0x%lx c=0x%lx\r\n", current2, c);
            KdBugcheck2(KERNEL_CORE_COMP_FAIL, NULL, __LINE__, __FILE__);
        }
        previous2->GlobalNext = current2->GlobalNext;
        current2->GlobalNext = NULL;
    }
    _3:
    c->ProcNext = NULL;
    c->GlobalNext = NULL;
    DeathThread = c;
    SpnLckRelease(&SchedSpinlock);
    SchedYield();
    __builtin_unreachable();
}

uint64_t SysKill(uint64_t pid, KE_SYSCALL_ARGS_UNUSED1) {
    if (pid == 0) return -1; // cant kill kernel process
    ProcessCtrlBlk* process = ProcFindByPid(pid, KernelGetInformation());
    if (!process) return -1; // no such process
    ThreadCtrlBlk* thrlist = process->ThreadListHead;
    if (!thrlist) return -1; // well somethings probably gone wrong (process is probably in the process of being killed)
    // set pending kill flag on all threads on process
    printf("ksyscall: SysKill: set kill flag on all threads for pid %d (kill syscall from process with pid %d)\r\n", pid, CurrentThread->ParentProc->pid);
    ThreadCtrlBlk* current = thrlist;
    while (current != NULL) {
        current->pendingkill = 1;
        current = current->ProcNext;
    }
    return 0;
}

uint64_t SysSpawn(uint64_t pathaddr, uint64_t argv, uint64_t argc, uint64_t envp, uint64_t envc) {
    const char* path = (const char*)pathaddr;
    int handle = OsOpen(path, 0);
    if (handle <= -1) return -1;
    uint64_t size = OsGetFileSize(handle);
    void* buf = MmAllocate(size);
    OsRead(handle, buf, size);
    OsClose(handle);
    uint64_t pid = 0;
    printf("ksyscall: SysSpawn: spawn new process\r\n");
    KSTATUS result = LdrElfExecute(buf, SCHED_PRIV_USER, &pid, (const char**)argv, (int)argc, (const char**)envp, (uint64_t)envc, basename(path));
    MmFree(buf);
    return (result == KSUCCESS) ? pid : -1;
}

uint64_t SysConWrite(uint64_t pathaddr, KE_SYSCALL_ARGS_UNUSED1) {
    const char* path = (const char*)pathaddr;
    UserAcBegin();
    printf("%s", path);
    UserAcEnd();
    return 0;
}

// should probably make a KE_SYSCALL_ARGS_UNUSED0
uint64_t SysYield(uint64_t arg1, KE_SYSCALL_ARGS_UNUSED1) {
    SchedYield();
    return 0;
}

uint64_t SysGetPid(uint64_t arg1, KE_SYSCALL_ARGS_UNUSED1) {
    return CurrentThread->ParentProc->pid;
}

uint64_t SysSBrk(uint64_t inc, KE_SYSCALL_ARGS_UNUSED1) {
    ProcessCtrlBlk* proc = CurrentThread->ParentProc;
    if (inc == 0) {
        return proc->SbrkCurrent;
    }
    uint64_t OldBrk = proc->SbrkCurrent;
    uint64_t NewBrk = OldBrk + inc;
    if (NewBrk < proc->SbrkBase || NewBrk > proc->SbrkLimit) {
        return -1;
    }
    if (inc > 0) {
        uint64_t OldEnd = MMU_ROUND_PAGE_UP(OldBrk);
        uint64_t NewEnd = MMU_ROUND_PAGE_UP(NewBrk);
        for (uint64_t VirtAddr = OldEnd; VirtAddr < NewEnd; VirtAddr+=MMU_PAGE_SIZE) {
            uint64_t Phys = (uint64_t)PmmAllocate();
            memset((void*)P2V(Phys), 0, PAGE_SIZE);
            MmuMapPage((pagetable*)P2V(proc->cr3), VirtAddr, Phys, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_US_USER);
        }
    } else {
        uint64_t OldEnd = MMU_ROUND_PAGE_UP(OldBrk);
        uint64_t NewEnd = MMU_ROUND_PAGE_UP(NewBrk);
        for (uint64_t VirtAddr = OldEnd; VirtAddr < NewEnd; VirtAddr+=MMU_PAGE_SIZE) {
            MmuUnmapPage((pagetable*)P2V(proc->cr3), VirtAddr);
        }
    }
    proc->SbrkCurrent = NewBrk;
    return OldBrk;
}

// osopen doesnt care about modes anyway
uint64_t SysOpen(uint64_t path, KE_SYSCALL_ARGS_UNUSED1) {
    char acpath[VFS_MAX_ALLOWED_PATH];
    int r2 = VfsTranslatePath((char*)path, (char*)acpath, CurrentThread->ParentProc);
    if (r2 < 0) return (uint64_t)-1;
    int h = OsOpen(acpath, arg2);
    return (uint64_t)h;
}

uint64_t SysClose(uint64_t handle, KE_SYSCALL_ARGS_UNUSED1) {
    int h = OsClose((int)handle);
    return (uint64_t)h;
}

uint64_t SysRead(uint64_t handle, uint64_t buffer, uint64_t nbytes, KE_SYSCALL_ARGS_UNUSED3) {
    int r = OsRead((int)handle, (void*)buffer, (uint64_t)nbytes);
    return (uint64_t)r;
}

uint64_t SysWrite(uint64_t handle, uint64_t buffer, uint64_t nbytes, KE_SYSCALL_ARGS_UNUSED3) {
    int r = OsWrite((int)handle, (const void*)buffer, (uint64_t)nbytes);
    return (uint64_t)r;
}

uint64_t SysWaitPid(uint64_t pid, KE_SYSCALL_ARGS_UNUSED1) {
    ProcessCtrlBlk* proc = ProcFindByPid(pid, KernelGetInformation());
    if (!proc) return (uint64_t)-1;
    ThreadCtrlBlk* thr = ThrGetCurrent();
    thr->state = SCHED_THREAD_SUSPENDED;
    ThreadPushTail(&proc->BlockedQueueHead, &proc->BlockedQueueTail, thr);
    SchedYield();
    return proc->exitcode;
}

uint64_t SysSeek(uint64_t handle, uint64_t offset, uint64_t whence, KE_SYSCALL_ARGS_UNUSED3) {
    ProcessCtrlBlk* proc = ThrGetCurrent()->ParentProc;
    if (handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return (uint64_t)-1;
    if (!proc->FileHandleTable[handle].Entry) return (uint64_t)-1;
    int64_t NewOff;
    switch (whence) {
        case SEEK_SET: NewOff = (int64_t)offset; break;
        case SEEK_CUR: NewOff = (int64_t)proc->FileHandleTable[handle].CursorPos + (int64_t)offset; break;
        case SEEK_END: NewOff = (int64_t)proc->FileHandleTable[handle].Entry->Size + (int64_t)offset; break;
        default: {
            printf("ksyscall: SysSeek: invalid whence value!\r\n");
            return (uint64_t)-1;
        }
    }
    if (NewOff < 0) return (uint64_t)-1;
    proc->FileHandleTable[handle].CursorPos = NewOff;
    return (uint64_t)NewOff;
}

extern uint64_t gCpuLapicTicksPer10ms;
extern uint64_t gCpuLapicTicksPerMs;
uint64_t SysGetClock(uint64_t clockid, uint64_t secondsOutPtr, uint64_t nanosecsOutPtr, KE_SYSCALL_ARGS_UNUSED3) {
    uint64_t TicksPerSec = gCpuLapicTicksPer10ms*100;
    uint64_t CurrentTick = CpuLapticTimerGetTick();
    uint64_t Seconds = CurrentTick / TicksPerSec;
    uint64_t Nanosecs = ((CurrentTick % TicksPerSec) * 1000000000ULL) / TicksPerSec;
    if (secondsOutPtr == 0 || nanosecsOutPtr == 0) return (uint64_t)-1;
    *(uint64_t*)secondsOutPtr = Seconds;
    *(uint64_t*)nanosecsOutPtr = Nanosecs;
    return 0;
}
uint64_t SysGetCwd(uint64_t buf, uint64_t size, KE_SYSCALL_ARGS_UNUSED2) {
    ProcessCtrlBlk* proc = CurrentThread->ParentProc;
    uint64_t length = strlen(proc->cwd)+1;
    if (length > size) return (uint64_t)-1;
    if (buf == 0) return (uint64_t)-1;
    memcpy((void*)buf, (const void*)proc->cwd, size);
    return buf;
}

uint64_t SysChdir(uint64_t path, KE_SYSCALL_ARGS_UNUSED1) {
    ProcessCtrlBlk* proc = CurrentThread->ParentProc;
    if (path == 0) return (uint64_t)-1;
    char acpath[VFS_MAX_ALLOWED_PATH];
    int r2 = VfsTranslatePath((char*)path, (char*)acpath, CurrentThread->ParentProc);
    if (r2 < 0) return (uint64_t)-1;
    int handle = OsOpen(acpath, 0);
    if (handle <= -1) return (uint64_t)-1;
    uint64_t type;
    OsStat(handle, NULL, &type); // extremely barebones stat. not even posix compliant
    if (type != VFS_TYPE_DIRECTORY) {
        OsClose(handle);
        return (uint64_t)-1;
    }
    OsClose(handle);
    strlcpy(proc->cwd, acpath, sizeof(proc->cwd));
    return 0;
}

uint64_t SysFstat(uint64_t handle, uint64_t statbuf, KE_SYSCALL_ARGS_UNUSED2) {
    if (handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return (uint64_t)-1;
    if (statbuf == 0) return (uint64_t)-1;
    posixstat stat;
    if (handle == VFS_HANDLE_STDOUT || handle == VFS_HANDLE_STDERR || handle == VFS_HANDLE_STDIN) {
        memset(&stat, 0, sizeof(posixstat));
        stat.st_mode = K_S_IFCHR;
        stat.st_blksize = 512;
        memcpy((void*)statbuf, &stat, sizeof(posixstat));
        return 0;
    }
    uint64_t type;
    uint64_t fsize;
    OsStat(handle, &fsize, &type);
    VfsFillStat(&stat, type, fsize);
    memcpy((void*)statbuf, (const void*)&stat, sizeof(posixstat));
    return 0;
}

uint64_t SysStat(uint64_t path, uint64_t statbuf, KE_SYSCALL_ARGS_UNUSED2) {
    if (path == 0) return (uint64_t)-1;
    char acpath[VFS_MAX_ALLOWED_PATH];
    int r2 = VfsTranslatePath((char*)path, (char*)acpath, CurrentThread->ParentProc);
    if (r2 < 0) return (uint64_t)-1;
    int h = OsOpen(acpath, 0);
    if (h < 0) return (uint64_t)-1;
    uint64_t r = KE_SYSCALL_CALL_ARG2(SysFstat, h, statbuf);
    OsClose(h);
    if (r == (uint64_t)-1) return (uint64_t)-1;
    return 0;
}

uint64_t SysGetDirent(uint64_t handle, uint64_t buffer, uint64_t maxsize, uint64_t bytesout, KE_SYSCALL_ARGS_UNUSED4) {
    ProcessCtrlBlk* proc = ThrGetCurrent()->ParentProc;
    if (handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return (uint64_t)-1;
    if (!proc->FileHandleTable[handle].Entry) return (uint64_t)-1;
    if (buffer == 0) return (uint64_t)-1;
    if (maxsize == 0) return (uint64_t)-1;
    if (bytesout == 0) return (uint64_t)-1;
    VfsDirEntry dirent;
    uint64_t currentpos = 0;
    uint64_t idx = proc->FileHandleTable[handle].CursorPos;
    void* tmpbuffer = MmAllocate(maxsize);
    while ((OsReadDir((int)handle, &dirent, idx)) == 1) {
        if ((currentpos + sizeof(VfsDirEntry)) > maxsize) {
            break;
        }
        posixdirent ent;
        memset(&ent, 0, sizeof(ent));
        ent.d_ino = 0;
        ent.d_off = (int64_t)(idx + 1);
        ent.d_reclen = sizeof(posixdirent);
        ent.d_type = (dirent.Type == VFS_TYPE_DIRECTORY) ? DT_DIR : DT_REG;
        strlcpy(ent.d_name, dirent.Name, sizeof(ent.d_name));
        memcpy((char*)tmpbuffer + currentpos, &ent, sizeof(posixdirent));
        currentpos += sizeof(posixdirent);
        idx++;
    }
    proc->FileHandleTable[handle].CursorPos = idx;
    memcpy((void*)buffer, tmpbuffer, currentpos);
    MmFree(tmpbuffer); // free working buffer
    return bytesout;
}

uint64_t SysUname(uint64_t buf, KE_SYSCALL_ARGS_UNUSED1) {
    if (buf == 0) return (uint64_t)-1;
    utsname uname;
    memset(&uname, 0, sizeof(utsname));
    strlcpy(uname.sysname, "ah5kos", sizeof(uname.sysname));
    strlcpy(uname.nodename, "<none>", sizeof(uname.nodename));
    strlcpy(uname.release, "1.0.0", sizeof(uname.release));
    strlcpy(uname.version, __DATE__ " " __TIME__, sizeof(uname.version));
    strlcpy(uname.machine, "x86_64", sizeof(uname.release));
    strlcpy(uname.domainname, "<none>", sizeof(uname.domainname));
    memcpy((void*)buf, &uname, sizeof(uname));
    return 0;
}

void KeRegisterSyscalls() {
    KiRegisterSyscall(OS_EXIT, SysExit);
    KiRegisterSyscall(OS_KILL, SysKill);
    KiRegisterSyscall(OS_SPAWN, SysSpawn);
    KiRegisterSyscall(OS_CONWRITE, SysConWrite); // todo: remove
    KiRegisterSyscall(OS_YIELD, SysYield);
    KiRegisterSyscall(OS_GETPID, SysGetPid);
    KiRegisterSyscall(OS_WAIT, SysWaitPid);
    KiRegisterSyscall(OS_SBRK, SysSBrk);
    KiRegisterSyscall(OS_OPEN, SysOpen);
    KiRegisterSyscall(OS_CLOSE, SysClose);
    KiRegisterSyscall(OS_READ, SysRead);
    KiRegisterSyscall(OS_WRITE, SysWrite);
    KiRegisterSyscall(OS_SEEK, SysSeek);
    KiRegisterSyscall(OS_GETCLOCK, SysGetClock);
    KiRegisterSyscall(OS_GETCWD, SysGetCwd);
    KiRegisterSyscall(OS_CHDIR, SysChdir);
    KiRegisterSyscall(OS_STAT, SysStat);
    KiRegisterSyscall(OS_FSTAT, SysFstat);
    KiRegisterSyscall(OS_GETDIRENT, SysGetDirent);
    KiRegisterSyscall(OS_UNAME, SysUname);
    #ifdef __x86_64__
    KiRegisterSyscalls64();
    #endif
}