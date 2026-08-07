#pragma once
#include <stddef.h>
#include <stdint.h>
#include <external/posix/stat.h>
#include <fs/io/pipe.h>

#define VFS_HANDLE_STDOUT 1
#define VFS_HANDLE_STDERR 2
#define VFS_HANDLE_STDIN 0

#define VFS_MAX_ALLOWED_PATH 64
#define VFS_MAX_ALLOWED_OPEN_HANDLES 64
struct VfsFile;
typedef struct VfsDirEntry {
    uint64_t Id;
    uint64_t Type;
    char Name[256];
    char Path[VFS_MAX_ALLOWED_PATH];
} VfsDirEntry;

typedef struct {
    int (*Open)(struct VfsFile*);
    int (*Read)(struct VfsFile*, void*, size_t, uint64_t); // IN file, buffer, nbytes to read, offset   OUT status flag (0 = success, anything else is failure)
    int (*Write)(struct VfsFile*, const void*, size_t, uint64_t); // IN file, buffer, nbytes to write, offset   OUT status flag
    int (*Close)(struct VfsFile*);
    int (*GetFileSize)(struct VfsFile*);
    struct VfsFile* (*FindFile)(const char*);
    int (*ReadDir)(struct VfsFile*, VfsDirEntry*, int); // IN file, OUT dirent, IN index
} VfsDriverOperation;

typedef struct {
    char Name[VFS_MAX_ALLOWED_PATH]; // accessing a file would be [VfsDrive->Name]:/[Path]
    VfsDriverOperation* DriverOps;
} VfsDrive;

#define VFS_TYPE_FILE 0x1
#define VFS_TYPE_DIRECTORY 0x2

typedef struct VfsFile {
    const char Path[VFS_MAX_ALLOWED_PATH]; // actual full path
    uint64_t Size; // file size in bytes
    uint64_t Type;
    uint64_t Perms; // useless cuz we dont even enforce any of them lmfao
    VfsDrive* DrivePtr; // ptr back to the drive it's on
} VfsFile;

#define VFS_OFD_FLAG_FILE 0x1
#define VFS_OFD_FLAG_PIPE 0x2
#define VFS_OFD_FLAG_CNSL 0x3

typedef struct {
    VfsFile* Entry; // only applies if Flag = VFS_OFD_FLAG_FILE otherwise NULL
    uint64_t CursorPos;
    int Flag;
    int NumProc;
    IoPipeObj* PipeEntry; // only applies if Flag = VFS_OFD_FLAG_PIPE otherwise NULL
} VfsOpenFileDescr;

void VfsListMountedDrives();
void VfsAddDriveToList(VfsDrive* drive);
char* VfsRemoveFormatPath(const char* in) ;
int VfsIsAbsolute(const char* in);
void VfsFillStat(posixstat* stat, uint64_t type, uint64_t size);
int VfsTranslatePath(char* path, char* acpath, struct ProcessCtrlBlk* proc);
VfsFile* VfsFindFile(const char* Path);

int OsOpen(const char* path, int flags);
int OsClose(int handle);
int OsRead(int handle, void* buffer, size_t nbytes);
int OsWrite(int handle, const void* buffer, size_t nbytes);
int OsGetFileSize(int handle);
int OsReadDir(int handle, VfsDirEntry* outdirent, int idx);
int OsStat(int handle, uint64_t* outsize, uint64_t* outtype);