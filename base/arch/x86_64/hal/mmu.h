#pragma once
#include "hal.h"
#include "../cpu/paging.h"
void HalSwPageTable(uint64_t new);
KSTATUS HalSetupMmu(KernelInformation* gkInfo);
pagetable* HalGetPageTable();
KSTATUS HalRmvIdentityMap();