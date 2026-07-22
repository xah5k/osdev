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
    uint64_t arg3 = registers->rcx;
    uint64_t arg4 = registers->r8;
    uint64_t arg5 = registers->r9;
    uint64_t result = 0;
    if (syscallnum >= KE_MAX_SYSCALL || !gSyscallTable[syscallnum]) {
        printf("archsyscall: invalid syscall.\r\n");
        registers->rax = -1;
        return;
    }
    result = gSyscallTable[syscallnum](arg1, arg2, arg3, arg4, arg5);
    registers->rax = result;
    if (CurrentThread->pendingkill) {
        KE_SYSCALL_CALL_ARG1(SysExit, -1);
    }
}