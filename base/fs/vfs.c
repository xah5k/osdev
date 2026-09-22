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
#include <ttyobj.h>
void VfsGetDrvFromPath(const char* Path, char* BufOut);
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

int VfsCreate(const char* Path, int Type) {
    for (int i = 0; i < 16; i++) {
        if (!gVfsDrives[i]) { continue; }
        char Buf[VFS_MAX_ALLOWED_PATH];
        memset(Buf, 0, VFS_MAX_ALLOWED_PATH);
        memcpy(Buf, Path, strlen(Path)+1);
        VfsGetDrvFromPath(Path, Buf);
        if (strcmpl(Buf, gVfsDrives[i]->Name, strlen(Buf)) == 0) {
            if (gVfsDrives[i]->DriverOps->Create) {
                int PassedType =  (Type == VFS_TYPE_DIRECTORY) ? 1 : 0;
                printf("vfs: create: type=%d passedtype=%d\r\n", Type, PassedType);
                return gVfsDrives[i]->DriverOps->Create(Path, PassedType);
            }
        }    
    }
    return -1;
}

VfsFile* VfsFindFile(const char* Path) {
    for (int i = 0; i < 16; i++) {
        if (!gVfsDrives[i]) { continue; }
        char Buf[VFS_MAX_ALLOWED_PATH];
        memset(Buf, 0, VFS_MAX_ALLOWED_PATH);
        memcpy(Buf, Path, strlen(Path)+1);
        VfsGetDrvFromPath(Path, Buf);
        if (strcmpl(Buf, gVfsDrives[i]->Name, strlen(Buf)) == 0) {
            if (gVfsDrives[i]->DriverOps->FindFile) {
                return gVfsDrives[i]->DriverOps->FindFile(Path);
            }
        }    
    }
    return NULL;
}

void VfsAddDriveToList(VfsDrive* drive) {
    if ((gVfsDrivesMounted+1) > 16) return;
    gVfsDrives[gVfsDrivesMounted] = drive;
    gVfsDrivesMounted++;
}

void VfsListMountedDrives() {
    for (int i = 0; i < 16; i++) {
        if (!gVfsDrives[i]) {continue;}
        printf("vfs: drive %d: name=%s\r\n", i, gVfsDrives[i]->Name);
    }
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
    return (char*)in;
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
// this is actually disgusting
void VfsGetDrvFromPath(const char* Path, char* BufOut) {
    if (Path == NULL) return;
    for (int i = 0; BufOut[i] != '\0'; i++) {
        if (BufOut[i] == ':') {
            BufOut[i] = '\0';
            return;
        }
    }
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
    if (!proc) return -1;
    if (path[0] == '.' && path[1] == '/') {
        uint64_t len __attribute__((unused)) = strlen((const char*)path)-2;
        char* ptr = path+2;
        snprintf((char*)acpath, VFS_MAX_ALLOWED_PATH, "%s/%s", proc->cwd, ptr);
        uint64_t nl = strlen(acpath);
        if (acpath[nl-1] == '/') {
            acpath[nl-1] = '\0';
        }
        return 0;
    }
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
    if ((flags & VFS_OFD_OFLAG_CREATE) == VFS_OFD_OFLAG_CREATE) {
        int fl = flags;
        fl &= ~(VFS_OFD_OFLAG_CREATE);
        int s = OsCreate(path, VFS_TYPE_FILE);
        if (s < 0) return -1;
        int h = OsOpen(path, fl);
        return h;
    }
    VfsFile* f = VfsFindFile(path);
    if (!f) return -1;
    VfsOpen(f);
    if (flags == 0) flags = VFS_OFD_OFLAG_RW;
    ProcessCtrlBlk* current = KernelGetCurrentProc();
    if (!current) { KernelUnlockRsLck(); return -1; }
    if (current->nextfh >= VFS_MAX_ALLOWED_OPEN_HANDLES) { KernelUnlockRsLck(); return -1; }
    int handle = current->nextfh;
    current->nextfh++;
    current->FileHandleTable[handle].Entry = f;
    current->FileHandleTable[handle].Flag = VFS_OFD_FLAG_FILE;
    current->FileHandleTable[handle].OpenFl = flags;
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
    if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_FILE && (((proc->FileHandleTable[handle].OpenFl & VFS_OFD_OFLAG_RO) == VFS_OFD_OFLAG_RO) || ((proc->FileHandleTable[handle].OpenFl & VFS_OFD_OFLAG_RW) == VFS_OFD_OFLAG_RW))) {
        if (!proc->FileHandleTable[handle].Entry)  { KernelUnlockRsLck(); return -1; }
        VfsFile* f = proc->FileHandleTable[handle].Entry;
        if (f->Type == VFS_TYPE_OBJECT) {
            // skip the offset bs
            KernelUnlockRsLck();
            return VfsRead(f, buffer, nbytes, 0);
        }
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
        return TtyRead(proc->TtyObj, buffer, nbytes);
    }
    KernelUnlockRsLck();
    return -1;
}

KE_EXPORT_SYMBOL(OsRead);
int OsWrite(int handle, const void* buffer, size_t nbytes) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (handle <= -1 || handle >= VFS_MAX_ALLOWED_OPEN_HANDLES)  { KernelUnlockRsLck(); return -1; }
    if (proc->FileHandleTable[handle].Flag == VFS_OFD_FLAG_FILE && (((proc->FileHandleTable[handle].OpenFl & VFS_OFD_OFLAG_WO) == VFS_OFD_OFLAG_WO) || ((proc->FileHandleTable[handle].OpenFl & VFS_OFD_OFLAG_RW) == VFS_OFD_OFLAG_RW))) {
        VfsFile* f = proc->FileHandleTable[handle].Entry;
        uint64_t FileSize __attribute__((unused)) = f->Size;
        uint64_t CurrentOff = proc->FileHandleTable[handle].CursorPos;

        KernelUnlockRsLck();
        int bytes_wrote = VfsWrite(f, buffer, nbytes, CurrentOff); 
        // printf("vfs: bytes_wrote=%d\r\n", bytes_wrote);
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
        return TtyWrite(proc->TtyObj, buffer, nbytes);
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

int OsCreate(const char* path, int type) {
    return VfsCreate(path, type);
}