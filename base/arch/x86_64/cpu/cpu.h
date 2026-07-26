#pragma once
#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rdx, rcx, rbx, rax, rbp;
    uint64_t intnum, errcode;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) CpuInterruptArgs;

typedef struct {
    uint8_t smap;
} CpuFeatures;

extern uint64_t _x86_64_get_stack();
extern void _x86_64_set_stack(uint64_t);
extern void _x86_64_pause();
void CpuDisablePic();

extern void _x86_64_ctxswitch(uint64_t*, uint64_t);
extern void _x86_64_usjmp(uint64_t, uint64_t, uint64_t, uint64_t); // userspace using iretq
#define IA32_APIC_BASE 0x1B
#define IA32_EFER 0xC0000080

uint64_t CpuReadMsr(uint64_t msr);
void CpuWriteMsr(uint64_t msr, uint64_t value) ;
CpuFeatures* CpuDetectFeatures() ;