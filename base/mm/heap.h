#pragma once
#include <stdint.h>

void MmHeapInitalize();
void* MmAllocate(uint64_t size);
void MmFree(void* ptr);
void MmHeapDumpMap();