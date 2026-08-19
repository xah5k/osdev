#pragma once
#include <stdint.h>
#include <kernel.h>
#include "cpu/paging.h"
// bunch of shi each arch needs to implement
KSTATUS HalInitalize(KernelInformation* kinfo);
KSTATUS HalInitalizeSerial();
KSTATUS HalPutChar(char c);
KSTATUS HalSetupMmu(KernelInformation* gkInfo);
pagetable* HalGetPageTable();
KSTATUS HalRmvIdentityMap();
void HalUserJump(uint64_t entry, uint64_t usersp, uint64_t userargv, uint64_t userargc);
uint64_t HalGetStack();
void HalContextSw(uint64_t* old, uint64_t new);
void HalSwPageTable(uint64_t new);