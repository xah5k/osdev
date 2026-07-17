#pragma once
#include <kernel.h>

typedef struct {
    const char* name;
    void* addr;
} KeExport;

#define KE_EXPORT_SYMBOL(func) \
    static const char __export_name_##func[] = #func; \
    __attribute__((section(".kexports"), used)) \
    const KeExport __export_##func = { \
        .name = __export_name_##func, \
        .addr = (void*)&func \
    }

void KeDrvWrite(const char* message);
void KeDrvWriteFmt(const char* message, ...);
const char** KeDrvBuildDriverList(int* countOut);