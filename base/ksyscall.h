#pragma once
#ifdef __x86_64__
#include <arch/x86_64/archsyscall.h>
#endif

uint64_t SysExit(uint64_t exitcode, KE_SYSCALL_ARGS_UNUSED1);
void KeRegisterSyscalls();
uint64_t SysSpawn(uint64_t pathaddr, uint64_t argv, uint64_t argc, uint64_t envp, uint64_t envc);
uint64_t SysWaitPid(uint64_t pid, KE_SYSCALL_ARGS_UNUSED1);
uint64_t SysGetCwd(uint64_t buf, uint64_t size, KE_SYSCALL_ARGS_UNUSED2);
uint64_t SysChdir(uint64_t path, KE_SYSCALL_ARGS_UNUSED1);
uint64_t SysStat(uint64_t path, uint64_t statbuf, KE_SYSCALL_ARGS_UNUSED2);

uint64_t SysKill(uint64_t pid, KE_SYSCALL_ARGS_UNUSED1);