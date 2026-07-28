#pragma once
#include <kernel.h>
struct KeDeviceObj;
struct KeIoRequest;
typedef struct {
    const char* name;
    void* addr;
} KeExport;

#define KE_DRIVER_MAX_DISPATCH 32

typedef struct KeDriverObj {
    char Name[32];
    void* Base;
    uint64_t Size;
    KSTATUS (*Initalize)(struct KeDriverObj*);
    void (*Unload)(struct KeDriverObj*);
    struct KeDriverObj* Next;
} KeDriverObj;

typedef struct KeDeviceObj {
    char Name[32];
    KeDriverObj* Owner;
    void* Device;
    KSTATUS(*Dispatch[KE_DRIVER_MAX_DISPATCH])(struct KeDeviceObj*, struct KeIoRequest*);
    struct KeDeviceObj* Next;
} KeDeviceObj;

typedef enum {
    IO_READ,
    IO_WRITE,
    IO_RW,
    IO_OPEN,
    IO_CLOSE,
} KeIoReqType;

typedef struct KeIoRequest {
    KeIoReqType Major;
    void* Buffer;
    uint64_t Length;
    uint64_t ReadBytes;
} KeIoRequest;

#define KE_EXPORT_SYMBOL(func) \
    static const char __export_name_##func[] = #func; \
    __attribute__((section(".kexports"), used)) \
    const KeExport __export_##func = { \
        .name = __export_name_##func, \
        .addr = (void*)&func \
    }
void* KeGetExport(const char* name);
void KeDrvWrite(const char* message);
void KeDrvWriteFmt(const char* message, ...);
const char** KeDrvBuildDriverList(int* countOut);
void KeDrvRegisterDriver(KeDriverObj* drv);
KeDriverObj* KeDrvFindDriverByName(const char* name);
KeDeviceObj* KeFindDeviceByName(const char* name);
void KeRegisterDevice(KeDeviceObj* dev);
KSTATUS KeIoDispatch(KeDeviceObj* device, KeIoRequest* ioreq);