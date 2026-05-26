#include <arch/x86_64/cpu/gdt.h>
#include <mm/vmm.h>
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) CpuGdtEntry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) CpuGdtr;

CpuGdtEntry Entries[3] = {
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0x9A, 0xAF, 0},
    {0, 0, 0, 0x92, 0xCF, 0},
};

void CpuInitalizeGdt() {
    CpuGdtr Gdtr;
    Gdtr.limit = (uint16_t)sizeof(Entries)-1;
    Gdtr.base = (uint64_t)&Entries;
    _x86_64_load_gdt((uint64_t)&Gdtr);
}