#include "archsyscall.h"
#include <printfwrapper.h>
#include <sched/process.h>
#include <ksyscall.h>

static syscallfunc gSyscallTable[KE_MAX_SYSCALL];
extern ThreadCtrlBlk* CurrentThread;
void KiRegisterSyscall(KiSyscallIdx index, syscallfunc ptr) {
    if (index > KE_MAX_SYSCALL) return;
    gSyscallTable[index] = ptr;
}
void KiHandleSyscall(CpuInterruptArgs* registers) {
    uint64_t syscallnum = registers->rax;
    uint64_t arg1 = registers->rdi;
    uint64_t arg2 = registers->rsi;
    uint64_t arg3 = registers->rdx;
    uint64_t arg4 = registers->rcx;
    uint64_t arg5 = registers->r8;
    uint64_t result = 0;
    if (syscallnum >= KE_MAX_SYSCALL || !gSyscallTable[syscallnum]) {
        printf("archsyscall: invalid syscall.\r\n");
        registers->rax = -1;
        return;
    }
    // special cuz it needs the whole register frame
    if (syscallnum == OS_FORK) {
        result = gSyscallTable[OS_FORK]((uint64_t)registers, 0, 0, 0, 0);
    } else  { result = gSyscallTable[syscallnum](arg1, arg2, arg3, arg4, arg5); }
    registers->rax = result;
    if (CurrentThread->pendingkill) {
        KE_SYSCALL_CALL_ARG1(SysExit, -1);
    }
}

uint64_t SysSetFsBase(uint64_t base, KE_SYSCALL_ARGS_UNUSED1) {
    CpuWriteMsr(0xC0000100, base);
    return 0;
}


void KiRegisterSyscalls64() {
    KiRegisterSyscall(OS_SETFSBASE, SysSetFsBase);
}