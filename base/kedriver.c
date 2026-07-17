#include <kedriver.h>
#include <util/util.h>
#include <printfwrapper.h>
#include <stdarg.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <memory.h>
extern const KeExport __start_kexports[];
extern const KeExport __end_kexports[];

void* KeGetExport(const char* name) {
    uint64_t TotalExports = (uint64_t)__end_kexports - (uint64_t)__start_kexports;
    for (uint64_t i = 0; i < TotalExports; i++) {
        if (strcmp(__start_kexports[i].name, name, strlen(__start_kexports[i].name)) == 0) {
            return __start_kexports[i].addr;
        }
    }
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
    const char* path = "initrd:/drivers";
    VfsDirEntry dirent;
    int idx = 0;
    int count = 0;
    int handle = OsOpen(path, 0);

    int result = OsReadDir(handle, &dirent, idx);
    
    while (result == 1) {
        result = OsReadDir(handle, &dirent, idx);
        const char* ext = KeDrvGetFileExt(dirent.Name);
        if (ext && strcmp(ext, "sys", 4) == 0) {
            count++;
            printf("kernel: buildlist: new count of %d drivers.\r\n", count);
        }
        idx++;
    }

    const char** Paths = (const char**)MmAllocate(sizeof(char*) * count);
    int i = 0;
    idx = 0;
    result = OsReadDir(handle, &dirent, idx);
    while (result == 1) {
        result = OsReadDir(handle, &dirent, idx);
        int len = strlen(dirent.Path)+1;
        const char* ext = KeDrvGetFileExt(dirent.Name);
        if (ext && strcmp(ext, "sys", 4) == 0) {
            Paths[i] = MmAllocate(len);
            memcpy(Paths[i], dirent.Path, len);
            i++;
            printf("kernel: buildlist: filled entry %d with driver '%s' \r\n", i, dirent.Path);
        }
        idx++;
    } 
    OsClose(handle);
    *countOut = count;
    return Paths;
}