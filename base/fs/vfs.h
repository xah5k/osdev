#pragma once
#include <stddef.h>
#include <stdint.h>


#define VFS_HANDLE_STDOUT 1
#define VFS_HANDLE_STDERR 2
#define VFS_HANDLE_STDIN 0

#define VFS_MAX_ALLOWED_PATH 64
#define VFS_MAX_ALLOWED_OPEN_HANDLES 12
struct VfsFile;
typedef struct VfsDirEntry {
    uint64_t Id;
    uint64_t Type;
    char Name[256];
    char Path[VFS_MAX_ALLOWED_PATH];
} VfsDirEntry;

typedef struct {
    int (*Open)(struct VfsFile*);
    int (*Read)(struct VfsFile*, void*, size_t); // IN file, buffer, nbytes to read   OUT status flag (0 = success, anything else is failure)
    int (*Write)(struct VfsFile*, const void*, size_t); // IN file, buffer, nbytes to write   OUT status flag
    int (*Close)(struct VfsFile*);
    int (*GetFileSize)(struct VfsFile*);
    struct VfsFile* (*FindFile)(const char*);
    int (*ReadDir)(struct VfsFile*, VfsDirEntry*, int); // IN file, OUT dirent, IN index
} VfsDriverOperation;

typedef struct {
    char* Name; // accessing a file would be [VfsDrive->Name]:/[Path]
    VfsDriverOperation* DriverOps;
} VfsDrive;

#define VFS_TYPE_FILE 0x1
#define VFS_TYPE_DIRECTORY 0x2

typedef struct VfsFile {
    const char Path[VFS_MAX_ALLOWED_PATH]; // actual full path
    uint64_t Size; // file size in bytes
    uint64_t Type;
    VfsDrive* DrivePtr; // ptr back to the drive it's on
} VfsFile;

typedef struct {
    VfsFile* Entry;
    uint64_t CursorPos;
    int Flag;
    int NumProc;
} VfsOpenFileDescr;

void VfsAddDriveToList(VfsDrive* drive);
char* VfsRemoveFormatPath(const char* in) ;

int OsOpen(const char* path, int flags);
int OsClose(int handle);
int OsRead(int handle, void* buffer, size_t nbytes);
int OsWrite(int handle, const void* buffer, size_t nbytes);
int OsGetFileSize(int handle);
int OsReadDir(int handle, VfsDirEntry* outdirent, int idx);