#include <arch/x86_64/ports.h>
#include "cpu.h"
#include <kedriver.h>
#include <mm/heap.h>
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

CpuFeatures* CpuDetectFeatures() {
    CpuFeatures* r = MmAllocate(sizeof(CpuFeatures));
    uint32_t eax, ebx, ecx, edx;
    eax = 7; ecx = 0;
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                          : "a"(7), "c"(0));
    r->smap = (ebx >> 20) & 1;
    return r;
}
