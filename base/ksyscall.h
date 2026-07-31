#pragma once
#ifdef __x86_64__
#include <arch/x86_64/archsyscall.h>
#endif

uint64_t SysExit(uint64_t exitcode, KE_SYSCALL_ARGS_UNUSED1);
void KeRegisterSyscalls();
uint64_t SysSpawn(uint64_t pathaddr, uint64_t argv, uint64_t argc, KE_SYSCALL_ARGS_UNUSED3);
uint64_t SysWaitPid(uint64_t pid, KE_SYSCALL_ARGS_UNUSED1);