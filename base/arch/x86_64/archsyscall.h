#pragma once
#include <arch/x86_64/cpu/cpu.h>

#define KE_MAX_SYSCALL 32

#define KE_SYSCALL_ARGS_UNUSED1 uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5
#define KE_SYSCALL_ARGS_UNUSED2 uint64_t arg3, uint64_t arg4, uint64_t arg5
#define KE_SYSCALL_ARGS_UNUSED3 uint64_t arg4, uint64_t arg5
#define KE_SYSCALL_ARGS_UNUSED4 uint64_t arg5

#define KE_SYSCALL_CALL_ARG1(f, x) f(x, 0, 0, 0, 0)
#define KE_SYSCALL_CALL_ARG2(f, x, y) f(x, y, 0, 0, 0)
#define KE_SYSCALL_CALL_ARG3(f, x, y, z) f(x, y, z, 0, 0)
#define KE_SYSCALL_CALL_ARG4(f, x, y, z, a) f(x, y, z, a, 0)
#define KE_SYSCALL_CALL_ARG5(f, x, y, z, a, b) f(x, y, z, a, b)


typedef uint64_t(*syscallfunc)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#include "syscallidx.h"

void KiRegisterSyscall(KiSyscallIdx index, syscallfunc ptr);
void KiHandleSyscall(CpuInterruptArgs* registers);
void KiRegisterSyscalls64();