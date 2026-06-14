#pragma once
#include <stdint.h>
#include <kernel.h>
KSTATUS MmHeapInitalize();
void* MmAllocate(uint64_t size);
void MmFree(void* ptr);
void MmHeapDumpMap();