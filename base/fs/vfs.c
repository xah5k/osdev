//#include "mm/heap.h"
#include "kernel.h"
#include "mm/heap.h"
#include "sched/process.h"
#include <fs/vfs.h>
#include <util/util.h>
#include <memory.h>
#include <external/printf.h>

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

int VfsRead(VfsFile* file, void* buffer, size_t nbytes) {
    if (file->DrivePtr->DriverOps->Read) return file->DrivePtr->DriverOps->Read((void*)file, buffer, nbytes);
    else return -1;
}
int VfsWrite(VfsFile* file, const void* buffer, size_t nbytes) {
    if (file->DrivePtr->DriverOps->Write) return file->DrivePtr->DriverOps->Write((void*)file, buffer, nbytes);
    else return -1;
}

int VfsGetFileSize(VfsFile* file) {
    if (file->DrivePtr->DriverOps->GetFileSize) return file->DrivePtr->DriverOps->GetFileSize((void*)file);
    else return -1;
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

int OsOpen(const char* path, int flags) {
    VfsFile* f = VfsFindFile(path);
    if (!f) return -1;
    VfsOpen(f);

    VfsOpenFileDescr* desc = MmAllocate(sizeof(VfsOpenFileDescr));
    desc->Entry = f;
    desc->CursorPos = 0;

    ProcessCtrlBlk* current = KernelGetCurrentProc();
    if (!current) return -1;
    int handle = current->nextfh;
    current->nextfh++;
    current->FileHandleTable[handle] = desc;
    KernelUnlockRsLck();
    return handle;
}

int OsClose(int handle) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (!proc) return -1;
    proc->FileHandleTable[handle] = NULL;
    proc->nextfh--;
    KernelUnlockRsLck();
    return 0;
}

int OsRead(int handle, void* buffer, size_t nbytes) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (!proc->FileHandleTable[handle]) return -1;
    VfsFile* f = proc->FileHandleTable[handle]->Entry;
    KernelUnlockRsLck();
    return VfsRead(f, buffer, nbytes);
}

int OsWrite(int handle, const void* buffer, size_t nbytes) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (!proc->FileHandleTable[handle]) return -1;
    VfsFile* f = proc->FileHandleTable[handle]->Entry;
    KernelUnlockRsLck();
    return VfsWrite(f, buffer, nbytes);
}

int OsGetFileSize(int handle) {
    ProcessCtrlBlk* proc = KernelGetCurrentProc();
    if (!proc->FileHandleTable[handle]) return -1;
    VfsFile* f = proc->FileHandleTable[handle]->Entry;
    KernelUnlockRsLck();
    return VfsGetFileSize(f);
}