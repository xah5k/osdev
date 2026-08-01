//#include "mm/heap.h"
#include "kernel.h"
#include "mm/heap.h"
#include "sched/process.h"
#include <fs/vfs.h>
#include <util/util.h>
#include <memory.h>
#include <external/printf.h>
#include <kedriver.h>
#include <util/kbdtransl.h>
#include <external/posix/stat.h>
// 16 max drives
VfsDrive* gVfsDrives[16];
int gVfsDrivesMounted = 0;
int VfsOpen(VfsFile* file) {
    if (file->DrivePtr->DriverOps->Open) return file->DrivePtr->DriverOps->Open((void*)file);
    else return -1;
}

int VfsClose(VfsFile* file) {
    if (file->DrivePtr->DriverOps->Close) return file->DrivePtr->DriverOps->Close((void*)file);
    else return -1;
}

int VfsRead(VfsFile* file, void* buffer, size_t nbytes, uint64_t offset) {
    if (file->DrivePtr->DriverOps->Read) return file->DrivePtr->DriverOps->Read((void*)file, buffer, nbytes, offset);
    else return -1;
}
int VfsWrite(VfsFile* file, const void* buffer, size_t nbytes, uint64_t offset) {
    if (file->DrivePtr->DriverOps->Write) return file->DrivePtr->DriverOps->Write((void*)file, buffer, nbytes, offset);
    else return -1;
}

int VfsGetFileSize(VfsFile* file) {
    if (file->DrivePtr->DriverOps->GetFileSize) return file->DrivePtr->DriverOps->GetFileSize((void*)file);
    else return -1;
}

int VfsReadDir(struct VfsFile* file, VfsDirEntry* outdir, int idx) {
    if (file->DrivePtr->DriverOps->ReadDir) return file->DrivePtr->DriverOps->ReadDir((void*)file, outdir, idx);
    else return -3;
}

VfsFile* VfsFindFile(const char* Path) {
    for (int i = 0; i < 16; i++) {
        if (!gVfsDrives[i]) { continue; }
        //printf("fs: vfs: gVfsDrives[%d]->DriverOps->FindFile @ 0x%p\r\n", i, gVfsDrives[i]->DriverOps->FindFile);
        if (gVfsDrives[i]->DriverOps->FindFile) return gVfsDrives[i]->DriverOps->FindFile(Path);        
    }
    return NULL;
}

void VfsAddDriveToList(VfsDrive* drive) {
    if ((gVfsDrivesMounted+1) > 16) return;
    gVfsDrives[gVfsDrivesMounted] = drive;
    gVfsDrivesMounted++;
}

char* VfsRemoveFormatPath(const char* in) {
    if (in == NULL) return NULL;
    char* p = (char*)in;
    while (*p != 0) {
        if (*p == ':') {
            return p+1;
        }
        p++;
    }
    return in;
}

int VfsIsAbsolute(const char* in) {
    if (in == NULL) return 0;
    if (in[0] == '/') return 1;
    char* p = (char*)in;
    while (*p != 0) {
        if (*p == ':') {
            return 1;
        }
        p++;
    }
    return 0;
}

void VfsFillStat(posixstat* stat, uint64_t type, uint64_t size) {
    memset(stat, 0, sizeof(posixstat));
    if (type == VFS_TYPE_DIRECTORY) {
        stat->st_mode = K_S_IFDIR | 755;
    } else {
        stat->st_mode = K_S_IFREG | 644;
    }
    stat->st_size = size;
    stat->st_nlink = 1;
    stat->st_blksize = 512;
    stat->st_blocks = (size + 511) / 512;
}

int VfsTranslatePath(char* path, char* acpath, ProcessCtrlBlk* proc) {
    if (!proc) return (uint64_t)-1;
    if (VfsIsAbsolute((const char*)path)) {
        uint64_t len = strlen((const char*)path);
        if (len > VFS_MAX_ALLOWED_PATH - 1) len = VFS_MAX_ALLOWED_PATH - 1;
        memcpy(acpath, (const void*)path, len);
        acpath[len] = '\0';
    } else {
        snprintf((char*)acpath, VFS_MAX_ALLOWED_PATH, "%s/%s", proc->cwd, (const char*)path);
    }
    return 0;
}

int OsOpen(const char* path, int flags) {
    VfsFile* f = VfsFindFile(path);
    if (!f) return -1;
    VfsOpen(f);

    ProcessCtrlBlk* current = KernelGetCurrentProc();
    if (!current) return -1;
    if (current->nextfh >= VFS_MAX_ALLOWED_OPEN_HANDLES) return -1;
    int handle = current->nextfh;
    current->nextfh++;
    current->FileHandleTable[handle].Entry = f;
    KernelUnlockRsLck();
    return handle;
}
KE_EXPORT_SYMBOL(OsOpen);
int OsClose(int handle) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (!proc) return -1;
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return -1;
    memset(&proc->FileHandleTable[handle], 0, sizeof(VfsOpenFileDescr));
    proc->nextfh--;
    KernelUnlockRsLck();
    return 0;
}
KE_EXPORT_SYMBOL(OsClose);
int OsRead(int handle, void* buffer, size_t nbytes) {
    if (handle == VFS_HANDLE_STDIN) {
        char c = KbdTranslGetc();
        memcpy(buffer, &c, 1);
        return 1;
    } else if (handle == VFS_HANDLE_STDOUT || handle == VFS_HANDLE_STDERR) {
        return -1;
    }

    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return -1;
    if (!proc->FileHandleTable[handle].Entry) return -2;
    VfsFile* f = proc->FileHandleTable[handle].Entry;
    uint64_t FileSize = f->Size;
    uint64_t CurrentOff = proc->FileHandleTable[handle].CursorPos;

    if (CurrentOff >= FileSize) {
        KernelUnlockRsLck();
        return 0; 
    }
    if (CurrentOff + nbytes > FileSize) {
        nbytes = FileSize - CurrentOff;
    }
    KernelUnlockRsLck();
    int bytes_read = VfsRead(f, buffer, nbytes, CurrentOff); 

    if (bytes_read > 0) {
        proc->FileHandleTable[handle].CursorPos += bytes_read;
    }

    return bytes_read;
}
KE_EXPORT_SYMBOL(OsRead);
int OsWrite(int handle, const void* buffer, size_t nbytes) {
    if (handle == VFS_HANDLE_STDOUT || handle == VFS_HANDLE_STDERR) {
        const char* buf = (const char*)buffer;
        for (uint64_t i = 0; i < nbytes; i++) {
            _putchar(buf[i]); // before printf wouldve caused weird glitch characters to print out.
        }
        return nbytes;
    }
    if (handle == VFS_HANDLE_STDIN) return -1; // ??? some people are morons
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return -1;
    if (!proc->FileHandleTable[handle].Entry) return -2;
    VfsFile* f = proc->FileHandleTable[handle].Entry;
    uint64_t FileSize = f->Size;
    uint64_t CurrentOff = proc->FileHandleTable[handle].CursorPos;

    KernelUnlockRsLck();
    int bytes_wrote = VfsWrite(f, buffer, nbytes, CurrentOff); 

    if (bytes_wrote > 0) {
        proc->FileHandleTable[handle].CursorPos += bytes_wrote;
    }

    return bytes_wrote;
}

KE_EXPORT_SYMBOL(OsWrite);
int OsGetFileSize(int handle) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return -1;
    if (!proc->FileHandleTable[handle].Entry) return -2;
    VfsFile* f = proc->FileHandleTable[handle].Entry;
    KernelUnlockRsLck();
    return VfsGetFileSize(f);
}
KE_EXPORT_SYMBOL(OsGetFileSize);

int OsReadDir(int handle, VfsDirEntry* outdirent, int idx) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return -1;
    if (!proc->FileHandleTable[handle].Entry) return -2;
    VfsFile* f = proc->FileHandleTable[handle].Entry;
    KernelUnlockRsLck();
    return VfsReadDir(f, outdirent, idx);
}
KE_EXPORT_SYMBOL(OsReadDir);

// extremely basic
int OsStat(int handle, uint64_t* outsize, uint64_t* outtype) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) return -1;
    if (!proc->FileHandleTable[handle].Entry) return -2;
    VfsFile* f = proc->FileHandleTable[handle].Entry;
    KernelUnlockRsLck();
    if (outsize) *outsize = f->Size;
    if (outtype) *outtype = f->Type;
    return 0;
}