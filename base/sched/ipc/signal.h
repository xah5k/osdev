#pragma once
#include <kernel.h>
#include <arch/x86_64/cpu/cpu.h>
#define KE_SIGLIST_MAX 32
// ignore
#define KE_SIGLIST_ADDR_DEFAULTIGN 0
// kernel default handler
#define KE_SIGLIST_ADDR_DEFAULTKHDL 1
typedef KSTATUS (*KeSignalHdlFunc)(int SigIdx);
// glorified wrapper around Handler address
typedef struct {
    KeSignalHdlFunc Handler;
    uint64_t Flags;
    uint64_t Mask;
} KeSignalHdlObj;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rdx, rcx, rbx, rax, rbp;
    uint64_t rip, rflags, rsp;
    uint64_t SigBlockedSet;
    uint64_t SigNum;
} __attribute__((packed)) KeSignalUserFrame;
struct ThreadCtrlBlk;
#define KE_SIGNAL_DEF_TERMINATE 0
#define KE_SIGNAL_DEF_COREDUMP 1 // treat as same as terminate
#define KE_SIGNAL_DEF_IGNORE 2
#define KE_SIGNAL_DEF_STOP 3 // todo
#define KE_SIGNAL_DEF_CONT 4 // todo
// initalizes default handlers
KSTATUS KeSignalInitDef(struct ProcessCtrlBlk* proc);
KSTATUS KeSignalRegister(int Index, KeSignalHdlFunc handler, struct ProcessCtrlBlk* proc);
KSTATUS KeSignalDeregister(int Index, struct ProcessCtrlBlk* proc);

int KeSignalDefAct(int SigIdx);

// switches to a signal
void KeSignalHandle(struct ThreadCtrlBlk* Tcb, int SigIdx, KeSignalHdlObj* Signal, CpuInterruptArgs* OldCtx);
void KeSignalRegisterSyscalls();