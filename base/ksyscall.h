#pragma once
#include <stdint.h>
#define KE_MAX_SYSCALL 64

#define KE_SYSCALL_ARGS_UNUSED1 uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5
#define KE_SYSCALL_ARGS_UNUSED2 uint64_t arg3, uint64_t arg4, uint64_t arg5
#define KE_SYSCALL_ARGS_UNUSED3 uint64_t arg4, uint64_t arg5
#define KE_SYSCALL_ARGS_UNUSED4 uint64_t arg5

#define KE_SYSCALL_CALL_ARG1(f, x) f(x, 0, 0, 0, 0)
#define KE_SYSCALL_CALL_ARG2(f, x, y) f(x, y, 0, 0, 0)
#define KE_SYSCALL_CALL_ARG3(f, x, y, z) f(x, y, z, 0, 0)
#define KE_SYSCALL_CALL_ARG4(f, x, y, z, a) f(x, y, z, a, 0)
#define KE_SYSCALL_CALL_ARG5(f, x, y, z, a, b) f(x, y, z, a, b)


uint64_t SysExit(uint64_t exitcode, KE_SYSCALL_ARGS_UNUSED1);
void KeRegisterSyscalls();
uint64_t SysSpawn(uint64_t pathaddr, uint64_t argv, uint64_t argc, uint64_t envp, uint64_t envc);
uint64_t SysWaitPid(uint64_t pid, KE_SYSCALL_ARGS_UNUSED1);
uint64_t SysGetCwd(uint64_t buf, uint64_t size, KE_SYSCALL_ARGS_UNUSED2);
uint64_t SysChdir(uint64_t path, KE_SYSCALL_ARGS_UNUSED1);
uint64_t SysStat(uint64_t path, uint64_t statbuf, KE_SYSCALL_ARGS_UNUSED2);
uint64_t SysFstat(uint64_t handle, uint64_t statbuf, KE_SYSCALL_ARGS_UNUSED2);
uint64_t SysKill(uint64_t pid, uint64_t sign, KE_SYSCALL_ARGS_UNUSED2);