#pragma once
#include "../cpu/paging.h"
#include <kernel.h>

#define HAL_GET_CR2(cr2out) asm volatile("mov %%cr2, %0" : "=r"(cr2out));

void HalSwPageTable(uint64_t new);
KSTATUS HalSetupMmu(KernelInformation* gkInfo);
pagetable* HalGetPageTable();
KSTATUS HalRmvIdentityMap();
void HalReloadCr3();