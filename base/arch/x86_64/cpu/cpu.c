#include <arch/x86_64/ports.h>
#include "cpu.h"

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

void CpuWriteMsr(uint64_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
    );
}