#pragma once
#include <arch/x86_64/cpu/cpu.h>

typedef uint64_t(*syscallfunc)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#include "syscallidx.h"

void KiRegisterSyscall(KiSyscallIdx index, syscallfunc ptr);
void KiHandleSyscall(CpuInterruptArgs* registers);
void KiRegisterSyscalls64();