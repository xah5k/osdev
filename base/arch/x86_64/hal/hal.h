#pragma once
#include <stdint.h>
#include <kernel.h>
struct ThreadCtrlBlk;

#define HAL_GET_IP(r) r->rip
#define HAL_GET_ERR(r) r->errcode
#define HAL_GET_INUM(r) r->intnum
#define HAL_HALT() while (1) { __asm__ volatile ("cli;hlt"); }
#define HAL_HALT_WITHINT() while (1) { __asm__ volatile ("hlt"); }
#define HAL_INT_OFF() __asm__ volatile ("cli");
#define HAL_INT_ON() __asm__ volatile ("sti");
#define HAL_GET_SP(r) r->rsp
#define HAL_GET_BP(bpout) asm ("movq %%rbp,%0" : "=r"(bpout) ::)
void HalUserJump(uint64_t entry, uint64_t usersp, uint64_t userargv, uint64_t userargc);
uint64_t HalGetStack();
void HalContextSw(uint64_t* old, uint64_t new);
void HalDumpRegisters(CpuInterruptArgs* registers);
void HalContextSwPrep(struct ThreadCtrlBlk* NextThr);
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rdx, rcx, rbx, rax, rbp;
    uint64_t rip, rflags, rsp;
    uint64_t SigBlockedSet;
    uint64_t SigNum;
} __attribute__((packed)) HalSignalUserFrame;

#define HAL_SIGNAL_COPY_REGS(in, out) do { \
    in->r15 = out->r15; \
    in->r14 = out->r14; \
    in->r13 = out->r13; \
    in->r12 = out->r12; \ 
    in->r11 = out->r11; \
    in->r10 = out->r10; \
    in->r9 = out->r9; \
    in->r8 = out->r8; \
    in->rdi = out->rdi; \
    in->rsi = out->rsi; \
    in->rdx = out->rdx; \ 
    in->rcx = out->rcx; \
    in->rbx = out->rbx; \
    in->rax = out->rax; \
    in->rbp = out->rbp; \
    in->rip = out->rip; \
    in->rflags = out->rflags; \
    in->rsp = out->rsp; \
} while (0);

typedef struct HalStackFr {
    struct HalStackFr* bp;
    uint64_t ip;
} HalStackFr;