#include "tar.h"
#include <memory.h>
#include <stddef.h>
#include <printfwrapper.h>

#include <util/util.h>
#include "mm/heap.h"
#include "vfs.h"


VfsDrive* gTarVfsDrive = NULL;
VfsFile* gTarFsEntries;
int gTarFsNumEntries = 0;

void* gTarInitrdPtr = NULL;
int TarFsOpen(struct VfsFile* file) {
    return 0;
}
int TarFsClose(struct VfsFile* file) {
    return 0;
}

int TarFsRead(struct VfsFile* file, void* buffer, size_t nbytes) {
    if (!gTarInitrdPtr || !gTarVfsDrive) return -1;
    if (file->Type != VFS_TYPE_FILE) return -1;
    char* DriverPath = VfsRemoveFormatPath(file->Path);
    // get the tar entry for the file
    //printf("fs: read: gTarInitrdPtr:0x%p\r\n", gTarInitrdPtr);
    TarFileEntry* FsEntry = TarFsLookup(gTarInitrdPtr, DriverPath);
    if (!FsEntry) return -1;
    char* raw = (char*)(FsEntry + 1);
    size_t read = nbytes;
    if (nbytes > oct2bin((unsigned char*)FsEntry->Size, 11)) {
        read = oct2bin((unsigned char*)FsEntry->Size, 11);
    }
    memcpy(buffer, raw, read);
    return 0;
}

struct VfsFile* TarFsFindFile(const char* Path) {
    for (int i = 0; i < gTarFsNumEntries; i++) {
        //printf("fs: tar: %s against %s\r\n", Path, gTarFsEntries[i].Path);
        if (memcmp(gTarFsEntries[i].Path, Path, strlen(gTarFsEntries[i].Path)+1) == 0) return &gTarFsEntries[i];
    }
    return NULL;
}

// just counts all the tar file entries
static void internalCountEntries() {
    if (!gTarInitrdPtr) return;
    gTarFsNumEntries = 0;
    TarFileEntry* ptr = (TarFileEntry*)gTarInitrdPtr;
    while (((unsigned char*)ptr)[0] != '\0') {
        if (memcmp(ptr->Indicator, "ustar", 5) != 0) {
            //printf("fs: tar: fail ustar\r\n");
            break; // error
        }
        int filesize = oct2bin((unsigned char*)ptr->Size, 11);
        int datablocks = (filesize + 511) / 512;
        ptr += (datablocks+1);
        gTarFsNumEntries++;
    }
}

void TarInitalizeVfs(void* archive) {
    gTarInitrdPtr = archive;
    gTarVfsDrive = MmAllocate(sizeof(VfsDrive));
    printf("fs: tar: gTarInitrdPtr=0x%lx (passed arg 'archive'=0x%lx)\r\n", gTarInitrdPtr, archive);
    VfsDriverOperation* DriverOps = MmAllocate(sizeof(VfsDriverOperation));
    DriverOps->Open = (void*)TarFsOpen;
    DriverOps->Close = (void*)TarFsClose;
    DriverOps->Read = (void*)TarFsRead;
    DriverOps->Write = NULL;
    DriverOps->FindFile = (void*)TarFsFindFile;
    gTarVfsDrive->DriverOps = DriverOps;
    memcpy(gTarVfsDrive->Name, "initrd", 7);
    printf("fs: tar: created new drive (base=0x%lx driverops=0x%lx) '%s:'\r\n", gTarVfsDrive, gTarVfsDrive->DriverOps, gTarVfsDrive->Name);
    internalCountEntries();
    printf("fs: tar: counted %d entries in initrd\r\n", gTarFsNumEntries);
    gTarFsEntries = (VfsFile*)MmAllocate(sizeof(VfsFile) * gTarFsNumEntries);
    printf("fs: tar: begin discovery of entries\r\n");
    // for every ustarentry we discover we add it to the list of vfsentries
    TarFileEntry* current = (TarFileEntry*)gTarInitrdPtr;
    for (int i = 0; i < gTarFsNumEntries; i++) {
        if (current->Filename[0] == 0) continue;
        if (memcmp(current->Filename, ".", 2) == 0) continue;
        if (memcmp(current->Filename, "..", 3) == 0) continue;
        snprintf((char*)gTarFsEntries[i].Path, VFS_MAX_ALLOWED_PATH, "%s:/%s", gTarVfsDrive->Name, current->Filename);
        if (current->Flag == '0') gTarFsEntries[i].Type = VFS_TYPE_FILE;
        if (current->Flag == '5') gTarFsEntries[i].Type = VFS_TYPE_DIRECTORY;
        int filesize = oct2bin((unsigned char*)current->Size, 11);
        gTarFsEntries[i].Size = filesize;
        gTarFsEntries[i].DrivePtr = gTarVfsDrive;
        //printf("fs: tar: file path in original entry: %s\r\n", current->Filename);
        int datablocks = (filesize + 511) / 512;
        current += (datablocks+1);
        //printf("fs: tar: populated vfs file entry @ 0x%lx with path %s and type 0x%x\r\n", &gTarFsEntries[i], gTarFsEntries[i].Path, gTarFsEntries[i].Type);

    }
    printf("fs: tar: end discovery of entries\r\n");
    VfsAddDriveToList(gTarVfsDrive);
    printf("fs: tar: added initrd drive to vfs internal list\r\n");
}

TarFileEntry* TarFsLookup(uint8_t* archive, char* filename) {
    char* cmpfilename = filename;
    if (cmpfilename[0] == '/') {
        if (cmpfilename[1] == '\0') {
            cmpfilename = "";
        }
        cmpfilename++;
    }
    uint8_t* cptr = archive;
    for (int i = 0; i < gTarFsNumEntries; i++) {
        TarFileEntry* ptr = (TarFileEntry*)cptr;
        //printf("header @ 0x%lx\r\n", ptr);
        if (ptr->Filename[0] == 0) break;
        //printf("compare %s with %s\r\n", cmpfilename, ptr->Filename);
        if (memcmp(cmpfilename, ptr->Filename, strlen(cmpfilename)+1) == 0) return ptr;
        int filesize = oct2bin((unsigned char*)ptr->Size, 11);
        int datablocks = (filesize + 511) / 512;
        cptr += (datablocks+1) * 512;
    }
    //printf("fs: tar: failed lookup\r\n");
    return NULL;
}