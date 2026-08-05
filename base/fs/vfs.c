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
    if (!current) { KernelUnlockRsLck(); return -1; }
    if (current->nextfh >= VFS_MAX_ALLOWED_OPEN_HANDLES) { KernelUnlockRsLck(); return -1; }
    int handle = current->nextfh;
    current->nextfh++;
    current->FileHandleTable[handle].Entry = f;
    current->FileHandleTable[handle].Flag = VFS_OFD_FLAG_FILE;
    KernelUnlockRsLck();
    return handle;
}
KE_EXPORT_SYMBOL(OsOpen);
int OsClose(int handle) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (!proc) { KernelUnlockRsLck(); return -1; }
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) { KernelUnlockRsLck(); return -1; }
    if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_PIPE) {
        IoPipeObj* pipe = proc->FileHandleTable[handle].PipeEntry;
        if (pipe) {
            pipe->RefCount--;
            if (pipe->RefCount == 0) {
                MmFree(pipe);
            }
        }
    }
    memset(&proc->FileHandleTable[handle], 0, sizeof(VfsOpenFileDescr));
    KernelUnlockRsLck();
    return 0;
}
KE_EXPORT_SYMBOL(OsClose);
int OsRead(int handle, void* buffer, size_t nbytes) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) { KernelUnlockRsLck(); return -1; }
    if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_FILE) {
        if (!proc->FileHandleTable[handle].Entry)  { KernelUnlockRsLck(); return -1; }
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
    } else if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_PIPE) {
        IoPipeObj* pipe = proc->FileHandleTable[handle].PipeEntry;
        if (!pipe)  { KernelUnlockRsLck(); return -1; }
        uint64_t BytesRequested = nbytes;
        uint64_t BytesTotal = pipe->Count;
        uint64_t BytesToRead = BytesRequested; // value memcpy will use in the end
        if (BytesTotal == 0) {
            KernelUnlockRsLck();
            return 0;
        }
        if (BytesTotal < BytesRequested) {
            BytesToRead = BytesTotal;
        }
        uint64_t FirstChunk = IO_PIPE_BUF_SZ - pipe->ReadPos; // bytes available before wrap
        if (FirstChunk >= BytesToRead) {
            // copy
            memcpy(buffer, (const void*)((uint64_t)pipe->Buffer + pipe->ReadPos), BytesToRead);
        } else {
            // split copy
            memcpy(buffer, (const void*)((uint64_t)pipe->Buffer + pipe->ReadPos), FirstChunk);
            memcpy((void*)((uint64_t)buffer + FirstChunk), (const void*)pipe->Buffer, BytesToRead - FirstChunk);
        }
        pipe->ReadPos = (pipe->ReadPos + BytesToRead) % IO_PIPE_BUF_SZ;
        pipe->Count -= BytesToRead;
        KernelUnlockRsLck();
        return BytesToRead;
    } else if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_CNSL) {
        KernelUnlockRsLck();
        char c = KbdTranslGetc();
        memcpy(buffer, &c, 1);
        return 1;
    }
    KernelUnlockRsLck();
    return -1;
}

KE_EXPORT_SYMBOL(OsRead);
int OsWrite(int handle, const void* buffer, size_t nbytes) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES)  { KernelUnlockRsLck(); return -1; }
    if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_FILE) {
        
        VfsFile* f = proc->FileHandleTable[handle].Entry;
        uint64_t FileSize = f->Size;
        uint64_t CurrentOff = proc->FileHandleTable[handle].CursorPos;

        KernelUnlockRsLck();
        int bytes_wrote = VfsWrite(f, buffer, nbytes, CurrentOff); 

        if (bytes_wrote > 0) {
            proc->FileHandleTable[handle].CursorPos += bytes_wrote;
        }

        return bytes_wrote;
    } else if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_PIPE) {
        IoPipeObj* pipe = proc->FileHandleTable[handle].PipeEntry;
        if (!pipe)  { KernelUnlockRsLck(); return -1; }

        uint64_t FreeSpace = IO_PIPE_BUF_SZ - pipe->Count;
        if (FreeSpace == 0) { KernelUnlockRsLck(); return 0; }

        uint64_t BytesToWrite = nbytes;
        if (BytesToWrite > FreeSpace) {
            BytesToWrite = FreeSpace;
        }
        uint64_t FirstChunk = IO_PIPE_BUF_SZ - pipe->WritePos;
        if (FirstChunk >= BytesToWrite) {
            memcpy((void*)((uint64_t)pipe->Buffer + pipe->WritePos), buffer, BytesToWrite);
        } else {
            memcpy((void*)((uint64_t)pipe->Buffer + pipe->WritePos), buffer, FirstChunk);
            memcpy((void*)pipe->Buffer, (const void*)((uint64_t)buffer + FirstChunk), BytesToWrite - FirstChunk);
        }

        pipe->WritePos = (pipe->WritePos + BytesToWrite) % IO_PIPE_BUF_SZ;
        pipe->Count += BytesToWrite;
        KernelUnlockRsLck();
        return BytesToWrite;
    } else if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_CNSL) {
        KernelUnlockRsLck();
        const char* buf = (const char*)buffer;
        for (uint64_t i = 0; i < nbytes; i++) {
            _putchar(buf[i]); // before printf wouldve caused weird glitch characters to print out.
        }
        return nbytes;
    }
    KernelUnlockRsLck();
    return -1;
}

KE_EXPORT_SYMBOL(OsWrite);
int OsGetFileSize(int handle) {
    ProcessCtrlBlk* proc =  KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES)  { KernelUnlockRsLck(); return -1; }
    if (!proc->FileHandleTable[handle].Entry) { KernelUnlockRsLck(); return -1; }
    VfsFile* f = proc->FileHandleTable[handle].Entry;
    KernelUnlockRsLck();
    return VfsGetFileSize(f);
}
KE_EXPORT_SYMBOL(OsGetFileSize);

int OsReadDir(int handle, VfsDirEntry* outdirent, int idx) {
    ProcessCtrlBlk* proc =  KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) { KernelUnlockRsLck(); return -1; }
    if (!proc->FileHandleTable[handle].Entry) { KernelUnlockRsLck(); return -1; }
    VfsFile* f = proc->FileHandleTable[handle].Entry;
    KernelUnlockRsLck();
    return VfsReadDir(f, outdirent, idx);
}
KE_EXPORT_SYMBOL(OsReadDir);

// extremely basic
int OsStat(int handle, uint64_t* outsize, uint64_t* outtype) {
    ProcessCtrlBlk* proc =  KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES) { KernelUnlockRsLck(); return -1; }
    if (!proc->FileHandleTable[handle].Entry) { KernelUnlockRsLck(); return -1; }
    VfsFile* f = proc->FileHandleTable[handle].Entry;
    KernelUnlockRsLck();
    if (outsize) *outsize = f->Size;
    if (outtype) *outtype = f->Type;
    return 0;
}