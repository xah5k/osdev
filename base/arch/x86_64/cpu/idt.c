#include "arch/x86_64/cpu/idt.h"
#include "arch/x86_64/cpu/cpu.h"
#include "kernel.h"
#include <printfwrapper.h>
#include <stdint.h>

#define IDT_FLAG_PRESENT 0x80
#define IDT_FLAG_GATE 0x0E
// (?) #define IDT_FLAG_USER 0xE0

// entry
typedef struct {
    uint16_t base_low;
    uint16_t kernel_cs; // cpu.asm:24 pushes our kernel cs
    uint8_t ist;
    uint8_t flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t none;
} __attribute__((packed)) CpuIdtEntry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) CpuIdtr;


CpuIdtEntry idt[256];
irqhandler handlers[256]; // 256 handlers
static  CpuIdtr idtr;

extern void _x86_64_load_idt(uint64_t idtr);

extern void* isr_stub_table[];


void CpuIdtAsmHandler(CpuInterruptArgs* registers) {
    if (handlers[registers->intnum]) {
        handlers[registers->intnum](registers);
    } else {
        KdBugcheck(UNREGISTERED_INTERRUPT, registers);
        while (1) { asm ("cli; hlt"); } // though kernel bugcheck should do this for us
    }
}

void CpuRegisterHandler(uint64_t index, irqhandler handler) {
    handlers[index] = handler;
}


void CpuIdtSetEntry(CpuIdtEntry* table, uint8_t index, void* base, uint8_t flags) {
    CpuIdtEntry* entry = &table[index];

    entry->base_low = (uint64_t)base & 0xFFFF;
    entry->kernel_cs = 0x08; // todo: if cpu.asm changes it then change it here too
    entry->ist = 0;
    entry->flags = flags;
    entry->base_mid = ((uint64_t)base >> 16) & 0xFFFF;
    entry->base_high = ((uint64_t)base >> 32) & 0xFFFFFFFF;
    entry->none = 0;
}

void CpuInitalizeIdt() {
    for (int i = 0; i < 256; i++) {
        CpuIdtSetEntry(idt, i, isr_stub_table[i], IDT_FLAG_PRESENT | IDT_FLAG_GATE);
    }
    idtr.limit = sizeof(idt)-1;
    idtr.base = (uint64_t)&idt;
    //printf("before\r\n");
    _x86_64_load_idt((uint64_t)&idtr);
    //printf("after\r\n");
}