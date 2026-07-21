#pragma once
#ifdef __x86_64__
#include <arch/x86_64/archsyscall.h>
#endif

uint64_t SysExit(uint64_t exitcode, KE_SYSCALL_ARGS_UNUSED1);
void KeRegisterSyscalls();
