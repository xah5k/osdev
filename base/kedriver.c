#include <kedriver.h>
#include <util/util.h>
#include <printfwrapper.h>
#include <stdarg.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <memory.h>
extern const KeExport __start_kexports[];
extern const KeExport __end_kexports[];
static KeDriverObj* gDriverListHead = NULL;
static KeDeviceObj* gDeviceListHead = NULL;
void KeDrvRegisterDriver(KeDriverObj* drv) {
    drv->Next = gDriverListHead;
    gDriverListHead = drv;
}

KeDriverObj* KeDrvFindDriverByName(const char* name) {
    for (KeDriverObj* d = gDriverListHead;; d = d->Next) {
        if (strcmp(d->Name, name) == 0) return d;
    }
    return NULL;
}

void* KeGetExport(const char* name) {
    uint64_t TotalExports = (uint64_t)__end_kexports - (uint64_t)__start_kexports;
    for (uint64_t i = 0; i < TotalExports; i++) {
        if (__start_kexports[i].name == 0) {
            printf("kedriver: GetExport: failed to search for export '%s'. export list end\r\n", name);
        }
        if (strcmp(__start_kexports[i].name, name) == 0) {
            return __start_kexports[i].addr;
        }
    }
    return NULL;
}

void KeRegisterDevice(KeDeviceObj* dev) {
    dev->Next = gDeviceListHead;
    gDeviceListHead = dev;
}
KE_EXPORT_SYMBOL(KeRegisterDevice);

KeDeviceObj* KeFindDeviceByName(const char* name) {
    for (KeDeviceObj* d = gDeviceListHead; d; d = d->Next)
        if (strcmp(d->Name, name) == 0) return d;
    return NULL;
}

KeDeviceObj* KeDevGetLinkedList() {return gDeviceListHead;}
KeDriverObj* KeDrvGetLinkedList() {return gDriverListHead;}
void KeListDevices() {
    KeDeviceObj* d = gDeviceListHead;
    while (d != NULL) {
        printf("kedriver: device: name='%s' driver name='%s'\r\n", d->Name, d->Owner->Name);
        d = d->Next;
    }
}

void KeListDrivers() {
    KeDriverObj* d = gDriverListHead;
    while (d != NULL) {
        printf("kedriver: driver: name='%s'\r\n", d->Name);
        d = d->Next;
    }
}

uint64_t KeCountDevices() {
    uint64_t c = 0;
    KeDeviceObj* d = gDeviceListHead;
    while (d != NULL) {
        c++;
        d = d->Next;
    }
    return c;
}

uint64_t KeCountDrivers() {
    uint64_t c = 0;
    KeDriverObj* d = gDriverListHead;
    while (d != NULL) {
        c++;
        d = d->Next;
    }
    return c;
}
KSTATUS KeIoDispatch(KeDeviceObj* device, KeIoRequest* ioreq) {
    if (!device || !ioreq) return KINVALID;
    KSTATUS (*handler)(struct KeDeviceObj*, struct KeIoRequest*) = device->Dispatch[ioreq->Major];
    if (!handler) return KUNSUPPORTED;
    return handler(device, ioreq);
}

// writes a non formatted message
void KeDrvWrite(const char* message) {
    printf(message);
}
KE_EXPORT_SYMBOL(KeDrvWrite);
// writes a formatted message
void KeDrvWriteFmt(const char* message, ...) {
    va_list va;
    va_start(va, message);
    vprintf(message, va);
    va_end(va);
}
KE_EXPORT_SYMBOL(KeDrvWriteFmt);
static const char *KeDrvGetFileExt(const char *filename) {
    uint64_t len = strlen(filename);
    for (uint64_t i = len - 1; i > 0; i--) {
        if (filename[i] == '.') {
            return &filename[i + 1];
        }
    }
    
    return NULL;
}

const char** KeDrvBuildDriverList(int* countOut) {
    const char* path = "initrd:/drivers/";
    VfsDirEntry dirent;
    int handle = OsOpen(path, 0);
    int idx = 0;
    int count = 0;
    int result = OsReadDir(handle, &dirent, idx);
    while (result == 1) {
        const char* ext = KeDrvGetFileExt(dirent.Name);
        if (ext && strcmpl(ext, "sys", 4) == 0) {
            count++;
        }
        idx++;
        result = OsReadDir(handle, &dirent, idx);
    }

    const char** Paths = (const char**)MmAllocate(sizeof(char*) * count);
    memset((void*)Paths, 0, sizeof(char*) * count);
    int i = 0;
    idx = 0;
    result = OsReadDir(handle, &dirent, idx);
    while (result == 1) {
        int len = strlen(dirent.Path)+1;
        const char* ext = KeDrvGetFileExt(dirent.Name);
        if (ext && strcmp(ext, "sys") == 0) {
            Paths[i] = MmAllocate(len);
            memcpy((void*)Paths[i], dirent.Path, len);
            i++;
        }
        idx++;
        result = OsReadDir(handle, &dirent, idx);
    } 
    OsClose(handle);
    *countOut = count;
    return Paths;
}