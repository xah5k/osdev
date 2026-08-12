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
#include <external/posix/ioctl.h>
#include <sched/ipc/signal.h>
#include <external/posix/signal.h>
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
    // handle exit
    CurrentThread->exitcode = exitcode;
    CurrentThread->ParentProc->exitcode = exitcode;
    // wake up anyone waiting
    ThreadCtrlBlk* blthr = ThreadPopHead(&CurrentThread->ParentProc->BlockedQueueHead, &CurrentThread->ParentProc->BlockedQueueTail);
    while (blthr != NULL) {
        ThreadWake(blthr);
        blthr = ThreadPopHead(&CurrentThread->ParentProc->BlockedQueueHead, &CurrentThread->ParentProc->BlockedQueueTail); // threadpophead nulls out globalnext
    }
    uint64_t r = SpnLckAcquireRfl(&SchedSpinlock);
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
    SpnLckReleaseRfl(&SchedSpinlock, r);
    SchedYield();
    __builtin_unreachable();
}

uint64_t SysKill(uint64_t pid, KE_SYSCALL_ARGS_UNUSED1) {
    if (pid == 0) return -1; // cant kill kernel process
    ProcessCtrlBlk* process = ProcFindByPid(pid, KernelGetInformation());
    if (!process) return -1; // no such process
    ThreadCtrlBlk* thrlist = process->ThreadListHead;
    if (!thrlist) return -1; // well somethings probably gone wrong (process is probably in the process of being killed)
    // send SIGKILL
    ThreadCtrlBlk* current = thrlist;
    while (current != NULL) {
        current->SigPendingSet |= (1ULL << SIGKILL);
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
    KSTATUS result = LdrElfExecute(buf, SCHED_PRIV_USER, &pid, (const char**)argv, (int)argc, (const char**)envp, (uint64_t)envc, basename(path));
    MmFree(buf);
    return (result == KSUCCESS) ? pid : -1;
}

uint64_t SysConWrite(uint64_t pathaddr, KE_SYSCALL_ARGS_UNUSED1) {
    const char* path = (const char*)pathaddr;
    UserAcBegin();
    for (int i = 0; i < strlen(path); i++) {
        _putchar(path[i]);
    }
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

uint64_t SysUmask(uint64_t mode, uint64_t modeout, KE_SYSCALL_ARGS_UNUSED2) {
    ProcessCtrlBlk* proc = CurrentThread->ParentProc;
    if (!proc) return (uint64_t)-1;
    if (modeout == 0) {
        proc->Mode = mode;
        return 0;
    }
    uint64_t OldMode = proc->Mode;
    proc->Mode = mode;
    *(uint64_t*)modeout = OldMode;
    return OldMode;
}

uint64_t SysIoCtl(uint64_t handle, uint64_t request, uint64_t arg, KE_SYSCALL_ARGS_UNUSED3) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return (uint64_t)-1;
    int IsTty = (handle == VFS_HANDLE_STDOUT || handle == VFS_HANDLE_STDERR || handle == VFS_HANDLE_STDIN);
    switch (request) {
        case TIOCGWINSZ: {
            if (!IsTty) return (uint64_t)-1;
            unixwinsize ws;
            ws.ws_row = 25;
            ws.ws_col = 80;
            ws.ws_xpixel = 0;
            ws.ws_ypixel = 0;
            memcpy((void*)arg, &ws, sizeof(unixwinsize));
            KernelUnlockRsLck();
            return 0;
        }
        default: {
            KernelUnlockRsLck();
            return (uint64_t)-1;
        }
    }
    KernelUnlockRsLck();
    return (uint64_t)-1;
}

uint64_t SysCrPipe(uint64_t fhsout, KE_SYSCALL_ARGS_UNUSED1) {
    ProcessCtrlBlk* proc = CurrentThread->ParentProc;
    if (!proc) return (uint64_t)-1;
    if (fhsout == 0) return (uint64_t)-1;
    IoPipeObj* pipe = IoCreatePipe(proc);
    if (!pipe) return (uint64_t)-1;
    int fhs[2];
    fhs[0] = pipe->ReadHandle;
    fhs[1] = pipe->WriteHandle;
    memcpy((void*)fhsout, fhs, sizeof(fhs));
    return 0;
}

uint64_t SysFork(uint64_t frame, KE_SYSCALL_ARGS_UNUSED1) {
    CpuInterruptArgs* iframe = (CpuInterruptArgs*)frame;
    ProcessCtrlBlk* cproc = CurrentThread->ParentProc;
    uint64_t r = ProcessCopy(cproc, CurrentThread, iframe);
    return r;
}

static int KeHlpCountV(const char* p[]) {
    int i = 0;
    while (p[i] != NULL) {
        i++;
    }
    return i;
}
uint64_t SysExecve(uint64_t patha, uint64_t argv, uint64_t envp, KE_SYSCALL_ARGS_UNUSED3) {
    const char* path = (const char*)patha;
    if (patha == 0) return (uint64_t)-1;
    int handle = OsOpen(path, 0);
    if (handle <= -1) return (uint64_t)-1;
    uint64_t size = OsGetFileSize(handle);
    void* buf = MmAllocate(size);
    OsRead(handle, buf, size);
    OsClose(handle);
    printf("ksyscall: SysExecve: replace current image.\r\n");
    KSTATUS result = LdrElfReplaceImage(ThrGetCurrent()->ParentProc, buf, (const char**)argv, (int)KeHlpCountV((const char**)argv), (const char**)envp, (int)KeHlpCountV((const char**)envp));
    if (result != KSUCCESS) return (uint64_t)-1; else return 0;
    __builtin_unreachable();
    return 0;
}

uint64_t SysDup(uint64_t oldhandle, KE_SYSCALL_ARGS_UNUSED1) {
    ProcessCtrlBlk* proc = CurrentThread->ParentProc;
    if (oldhandle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return (uint64_t)-1;
    if (!proc->FileHandleTable[oldhandle].Entry && proc->FileHandleTable[oldhandle].Flag != VFS_OFD_FLAG_PIPE) return (uint64_t)-1;
    if (proc->nextfh >= VFS_MAX_ALLOWED_OPEN_HANDLES) return (uint64_t)-1;
    int newhdl = proc->nextfh++;
    proc->FileHandleTable[newhdl] = proc->FileHandleTable[oldhandle];
    // refcount 
    if (proc->FileHandleTable[newhdl].Flag == VFS_OFD_FLAG_PIPE) {
        proc->FileHandleTable[newhdl].PipeEntry->RefCount++;
    }
    return (uint64_t)newhdl;
}

uint64_t SysDup2(uint64_t oldhandle, uint64_t newhandle, KE_SYSCALL_ARGS_UNUSED2) {
    ProcessCtrlBlk* proc = CurrentThread->ParentProc;
    if (oldhandle >= VFS_MAX_ALLOWED_OPEN_HANDLES || newhandle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return (uint64_t)-1;
    if (!proc->FileHandleTable[oldhandle].Entry && proc->FileHandleTable[oldhandle].Flag != VFS_OFD_FLAG_PIPE) return (uint64_t)-1;
    if (proc->nextfh >= VFS_MAX_ALLOWED_OPEN_HANDLES) return (uint64_t)-1;
    if (oldhandle == newhandle) return newhandle;
    int newhdl = (int)newhandle;
    if (proc->FileHandleTable[newhdl].Entry || proc->FileHandleTable[newhdl].Flag == VFS_OFD_FLAG_FILE) {
        OsClose(newhdl);
    }

    proc->FileHandleTable[newhdl] = proc->FileHandleTable[oldhandle];
    // refcount 
    if (proc->FileHandleTable[newhdl].Flag == VFS_OFD_FLAG_PIPE) {
        proc->FileHandleTable[newhdl].PipeEntry->RefCount++;
    }
    return (uint64_t)newhdl;
}

uint64_t SysGetPpid(uint64_t arg1, KE_SYSCALL_ARGS_UNUSED1) {
    return ThrGetCurrent()->ParentProc->Parent->pid;
}

uint64_t SysAccess(uint64_t patha, uint64_t exist, uint64_t readp, uint64_t writep, uint64_t execp) {
    const char* path = (const char*)patha;
    if (writep) {
        return (uint64_t)-2;
    }
    if (exist) {
        int h = OsOpen(path, 0);
        if (h < 0) return (uint64_t)-1; // doesnt exist
    }
    if (readp) {
        int h = OsOpen(path, 0);
        if (h < 0) return (uint64_t)-1;
        return 0;
    }
    if (execp) {
        int h = OsOpen(path, 0);
        if (h < 0) return (uint64_t)-1;
        return 0;
    }
    return (uint64_t)-1;
}

uint64_t SysGetTermAttr(uint64_t IsStdinBuffer, uint64_t IsStdoutBuffer, KE_SYSCALL_ARGS_UNUSED2) {
    *(uint64_t*)IsStdinBuffer = ThrGetCurrent()->ParentProc->TtyObj->TtyInfo->StdinBufferFl;
    *(uint64_t*)IsStdoutBuffer = ThrGetCurrent()->ParentProc->TtyObj->TtyInfo->StdoutBufferFl;
    return 0;
}

uint64_t SysSetTermAttr(uint64_t IsStdinBuffer, uint64_t IsStdoutBuffer, KE_SYSCALL_ARGS_UNUSED2) {
    KeTtyInfo* TtyInfo = ThrGetCurrent()->ParentProc->TtyObj->TtyInfo;
    TtyInfo->StdinBufferFl = IsStdinBuffer;
    TtyInfo->StdoutBufferFl = IsStdoutBuffer;
    return 0;
}

// stupid shi for max arg 5 limit
typedef struct {
    void* addr;
    uint64_t length;
    int protect;
    int flags;
    int fd;
    uint64_t offset;
} MmapArgs;

uint64_t SysMmap(uint64_t structptr, KE_SYSCALL_ARGS_UNUSED1) {
    MmapArgs* args = (MmapArgs*)structptr;
    if (!args) return (uint64_t)-1;
    if (args->fd == -1 && args->flags & 0x20) {
        MmapEntry* e = MmAllocate(sizeof(MmapEntry));
        memset(e, 0, sizeof(MmapEntry));
        uint64_t length = MMU_ROUND_PAGE_UP(args->length);
        if (args->addr == 0x0) {
            args->addr = (void*)ThrGetCurrent()->ParentProc->MmapBumpNext;
            ThrGetCurrent()->ParentProc->MmapBumpNext += length;
        }
        for (int i = 0; i < length; i+=PAGE_SIZE) {
            void* physframe = PmmAllocate();
            memset((void*)P2V(physframe), 0, MMU_PAGE_SIZE);
            MmuMapPage((pagetable*)P2V(ThrGetCurrent()->ParentProc->cr3), ((uint64_t)args->addr + i), (physaddr)physframe, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_US_USER); // todo actually set perms based off the args passed
        }
        e->Vaddr = (virtaddr)args->addr;
        e->Next = ThrGetCurrent()->ParentProc->MmapEntryHead;
        ThrGetCurrent()->ParentProc->MmapEntryHead = e;
        return (uint64_t)args->addr;
    }
    return (uint64_t)-1;
}

uint64_t SysMunmap(uint64_t addr, uint64_t length, KE_SYSCALL_ARGS_UNUSED2) {
    MmapEntry* e = ThrGetCurrent()->ParentProc->MmapEntryHead;
    while (e != NULL) {
        if (e->Vaddr == addr && e->Length == length) {
            for (int i = 0; i < e->Length; i+=PAGE_SIZE) {
                MmuUnmapPage((pagetable*)P2V(ThrGetCurrent()->ParentProc->cr3), ((uint64_t)e->Vaddr + i));
            }
            return 0;
        }
        e = e->Next;
    }
    return -1;
}

void KeRegisterSyscalls() {
    KiRegisterSyscall(OS_EXIT, SysExit);
    KiRegisterSyscall(OS_KILL, SysKill);
    KiRegisterSyscall(OS_SPAWN, SysSpawn);
    KiRegisterSyscall(OS_CONWRITE, SysConWrite); // todo: remove
    KiRegisterSyscall(OS_YIELD, SysYield);
    KiRegisterSyscall(OS_GETPID, SysGetPid);
    KiRegisterSyscall(OS_GETPPID, SysGetPpid);
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
    KiRegisterSyscall(OS_UMASK, SysUmask);
    KiRegisterSyscall(OS_IOCTL, SysIoCtl);
    KiRegisterSyscall(OS_CRPIPE, SysCrPipe);
    KiRegisterSyscall(OS_FORK, SysFork);
    KiRegisterSyscall(OS_EXECVE, SysExecve);
    KiRegisterSyscall(OS_DUP, SysDup);
    KiRegisterSyscall(OS_DUP2, SysDup2);
    KiRegisterSyscall(OS_ACCESS, SysAccess);
    KiRegisterSyscall(OS_GTERMINFO, SysGetTermAttr);
    KiRegisterSyscall(OS_STERMINFO, SysSetTermAttr);
    KiRegisterSyscall(OS_MMAP, SysMmap);
    KiRegisterSyscall(OS_MUNMAP, SysMunmap);
    KeSignalRegisterSyscalls();
    #ifdef __x86_64__
    KiRegisterSyscalls64();
    #endif
}