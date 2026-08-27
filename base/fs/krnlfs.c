#include <fs/vfs.h>
#include <fs/krnlfs.h>
#include <util/util.h>
#include <mm/heap.h>
#include <kedriver.h>
#include <mouse.h>
#include <memory.h>
#include <external/printf.h>

static VfsDrive* gFsDrive;
static VfsFile* gFsEntries = NULL;
static int gFsNumEntries = 0;

static const char* KrnlFsRootDirs[3] = {"Devices", "Drivers", "Processes"};

int KrnlFsRead(struct VfsFile* file, void* buffer, size_t nbytes, uint64_t offset) {
    int read = -1;
    if (strcmpl(file->Path, "krnlfs:/Devices/", 16) == 0) {
        char* DevName = file->Path + 16;
        KeDeviceObj* d = KeFindDeviceByName(DevName);
        if (!d) {
            return -1;
        }
        KeIoRequest Irp;
        Irp.Buffer = (void*)buffer;
        Irp.Length = nbytes;
        Irp.ReadBytes = 0;
        Irp.Major = IO_READ;
        KSTATUS s = KeIoDispatch(d, &Irp);
        // printf("krnlfs: read on device returns KSTATUS 0x%lx\r\n", s);
        if (s != KSUCCESS) return -1;
        read = Irp.ReadBytes;
        // printf("krnlfs: read=%d\r\n", read);
        return read;
    } else if (strcmpl(file->Path, "krnlfs:/Drivers/", 16) == 0) {
        char* DrvName = file->Path + 16;
        KeDriverObj* d = KeDrvFindDriverByName(DrvName);
        if (!d) return -1;
        char cbuffer[1024];
        memset(cbuffer, 0, 1024);
        int w = snprintf(cbuffer, 1024, "name: %s\r\ninitalize: 0x%lx\r\nnext dobj: 0x%lx\r\n", d->Name, d->Initalize, d->Next);
        memcpy((void*)buffer, cbuffer, w);
        return w;
    } else if (strcmpl(file->Path, "krnlfs:/Processes/", 18) == 0) {
        char* ProcName = file->Path + 18;
        KeDrvWriteFmt("krnlfs: ProcName=\"%s\"\r\n", ProcName); 
    }
    return read;
}

static void KrnlFsInsertEntry(int i, char* Path, uint64_t Type, uint64_t Size) {
    VfsFile* file = MmAllocate(sizeof(VfsFile));
    memset((void*)file, 0, sizeof(VfsFile));
    sprintf(file->Path, "%s", Path);
    file->Type = Type;
    file->DrivePtr = gFsDrive;
    file->Size = Size;
    file->Next = gFsEntries;
    gFsEntries = file;
}

struct VfsFile* KrnlFsFindFile(const char* Path) {
    VfsFile* Current = gFsEntries;
    while (Current != NULL) {
        if (strcmpl(Current->Path, Path, VFS_MAX_ALLOWED_PATH) == 0) return Current;
        Current = Current->Next;
    }
    return NULL;
}


int KrnlFsReadDir(VfsFile* File, VfsDirEntry* OutDirEnt, int Index) {
    uint64_t DirLen = strlen(File->Path);
    int matches = 0;
    VfsFile* Current2 = gFsEntries;
    for (int i = 0; i < gFsNumEntries; i++) {
        if (Current2 == NULL) return 0;
        VfsFile* Current = Current2;
        Current2 = Current2->Next;
        if (Current->Path[0] == '\0') {
            break; 
        }
        const char* EntryPath = Current->Path;
        if (strcmpl(EntryPath, File->Path, DirLen) != 0) continue;
        if (strcmpl(EntryPath, File->Path, DirLen) == 0 && DirLen == strlen(EntryPath)) continue;
        const char* RelativePart = EntryPath + DirLen;
        if (RelativePart[0] == '/') RelativePart++;
        int IsNest = 0;
        int j = 0;
        while (RelativePart[j] != '\0') {
            if (RelativePart[j] == '/') {
                if (RelativePart[j + 1] != '\0') {
                    IsNest = 1;
                    break;
                }
            }
            j++;
        }
        if (IsNest) continue;
        if (matches == Index) {
            OutDirEnt->Id = i;
            OutDirEnt->Type = Current->Type;
            memcpy(OutDirEnt->Name, RelativePart, strlen(RelativePart));
            memcpy(OutDirEnt->Path, EntryPath, strlen(EntryPath));
            OutDirEnt->Name[strlen(RelativePart)] = 0;
            OutDirEnt->Path[strlen(EntryPath)] = 0;
            return 1;
        }
        matches++;
    }
    return 0;
}

int KrnlFsOpen(struct VfsFile* file) {
    return 0;
}
int KrnlfsClose(struct VfsFile* file) {
    return 0;
}

int KrnlFsGetFileSz(struct VfsFile* file) {
    return file->Size; // ?
}

void KrnlFsInitalize() {
    gFsDrive = MmAllocate(sizeof(VfsDrive));
    VfsDriverOperation* DriverOps = MmAllocate(sizeof(VfsDriverOperation));
    memset((void*)DriverOps, 0, sizeof(VfsDriverOperation));
    DriverOps->Open = (void*)KrnlFsOpen;
    DriverOps->Close = (void*)KrnlfsClose;
    DriverOps->GetFileSize = (void*)KrnlFsGetFileSz;
    DriverOps->Read = (void*)KrnlFsRead;
    DriverOps->Write = NULL;
    DriverOps->FindFile = (void*)KrnlFsFindFile;
    DriverOps->ReadDir = (void*)KrnlFsReadDir;
    gFsDrive->DriverOps = DriverOps;
    sprintf(gFsDrive->Name, "krnlfs");
    // populate dirs
    KrnlFsInsertEntry(0, "krnlfs:/", VFS_TYPE_DIRECTORY, 4096);
    gFsNumEntries++;
    for (int i = 0; i < 3; i++, gFsNumEntries++) {
        char buffer[VFS_MAX_ALLOWED_PATH];
        snprintf(buffer, VFS_MAX_ALLOWED_PATH, "krnlfs:/%s", KrnlFsRootDirs[i]);
        KrnlFsInsertEntry(gFsNumEntries, buffer, VFS_TYPE_DIRECTORY, 4096);
    }
    uint64_t DevCount = KeCountDevices();
    uint64_t DriverCount = KeCountDrivers();
    KeDeviceObj* device = KeDevGetLinkedList();
    for (int i = 0; i < DevCount; i++, gFsNumEntries++) {
        char buffer[VFS_MAX_ALLOWED_PATH];
        snprintf(buffer, VFS_MAX_ALLOWED_PATH, "krnlfs:/Devices/%s", device->Name);
        KrnlFsInsertEntry(gFsNumEntries, buffer, VFS_TYPE_OBJECT, 4096);
        device = device->Next;
    }
    KeDriverObj* driver = KeDrvGetLinkedList();
    for (int i = 0; i < DriverCount; i++, gFsNumEntries++) {
        char buffer[VFS_MAX_ALLOWED_PATH];
        if (strcmpl(driver->Name, "", 1) != 0) snprintf(buffer, VFS_MAX_ALLOWED_PATH, "krnlfs:/Drivers/%s", driver->Name);
        else {
            snprintf(buffer, VFS_MAX_ALLOWED_PATH, "krnlfs:/Drivers/_driver_unnamed_%d", i);
        }
        KrnlFsInsertEntry(gFsNumEntries, buffer, VFS_TYPE_OBJECT, 4096);
        driver = driver->Next;
    }
    VfsAddDriveToList(gFsDrive);
}