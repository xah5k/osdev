#include <arch/x86_64/cpu/gdt.h>
#include <mm/heap.h>
#include <memory.h>
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

typedef struct {
    uint32_t reserved0; 
    uint64_t rsp0;      
    uint64_t rsp1;
    uint64_t rsp2;      
    uint64_t reserved1; 
    uint64_t ist1;
    uint64_t ist2;      
    uint64_t ist3;      
    uint64_t ist4;
    uint64_t ist5;      
    uint64_t ist6;      
    uint64_t ist7;
    uint64_t reserved2; 
    uint16_t reserved3; 
    uint16_t iopb_offset;
} __attribute__((packed)) CpuTss;

CpuGdtEntry Entries[7] = {
    {0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0x9A, 0xAF, 0},
    {0, 0, 0, 0x92, 0xCF, 0},
    {0, 0, 0, 0xF2, 0xCF, 0},
    {0, 0, 0, 0xFA, 0xAF, 0},
    {0, 0, 0, 0x89, 0x00, 0},
    {0, 0, 0, 0x00, 0x00, 0}
};

void CpuInitalizeGdt() {
    CpuTss* Tss = MmAllocate(sizeof(CpuTss));
    memset((void*)Tss, 0, sizeof(CpuTss));
    uint64_t TssBase = (uint64_t)Tss;
    Entries[5].base_low = TssBase & 0xffff;
    Entries[5].base_middle = (TssBase >> 16) & 0xff;
    Entries[5].base_high = (TssBase >> 24) & 0xff;
    Entries[5].limit_low = sizeof(Tss);
    Entries[6].limit_low = (TssBase >> 32) & 0xffff;
    Entries[6].base_low = (TssBase >> 48) & 0xffff;
    
    CpuGdtr Gdtr;
    Gdtr.limit = (uint16_t)sizeof(Entries)-1;
    Gdtr.base = (uint64_t)&Entries;
    _x86_64_load_gdt((uint64_t)&Gdtr);
}