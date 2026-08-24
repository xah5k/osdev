#include <arch/x86_64/ports.h>
#include "cpu.h"
#include <kedriver.h>
#include <mm/heap.h>
#include <cpuid.h>
#include <memory.h>

void CpuDisablePic() {
    outb(0xA1, 0xFF);
    outb(0x21, 0xFF);
}

uint64_t CpuReadMsr(uint64_t msr) {
    uint32_t low, high;
    asm volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
	return ((uint64_t)high << 32) | low;
}
KE_EXPORT_SYMBOL(CpuReadMsr);

void CpuWriteMsr(uint64_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
    );
}
KE_EXPORT_SYMBOL(CpuWriteMsr);

static uint8_t CpuDetectSmap() {
    uint32_t eax, ebx, ecx, edx;
    eax = 7; ecx = 0;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                          : "a"(7), "c"(0));
    return (ebx >> 20) & 1;
}

static void CpuGetVendorId(CpuFeatures* features) {
    unsigned int eax, ebx, ecx, edx;
    __cpuid(0, eax, ebx, ecx, edx);
    memcpy(features->VendorId, &ebx, 4);
    memcpy(features->VendorId + 4, &edx, 4);
    memcpy(features->VendorId + 8, &ecx, 4);
    features->VendorId[12] = '\0';
}

static void CpuGetBrandStr(CpuFeatures* features) {
    unsigned int eax, ebx, ecx, edx;
    memset((void*)features->Brand, 0, 49);
    if (!__get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx) || eax < 0x80000004) {
        strlcpy(features->Brand, "N/A", 4);
        return;
    }
    __get_cpuid(0x80000002, &eax, &ebx, &ecx, &edx);
    memcpy((void*)features->Brand, &eax, 16);
    __get_cpuid(0x80000003, &eax, &ebx, &ecx, &edx);
    memcpy((void*)features->Brand + 16, &eax, 16);
    __get_cpuid(0x80000004, &eax, &ebx, &ecx, &edx);
    memcpy((void*)features->Brand + 32, &eax, 16);
    features->Brand[49] = '\0';
}

CpuFeatures* CpuDetectFeatures() {
    CpuFeatures* r = MmAllocate(sizeof(CpuFeatures));    
    r->smap = CpuDetectSmap();
    CpuGetVendorId(r);
    CpuGetBrandStr(r);
    return r;
}
