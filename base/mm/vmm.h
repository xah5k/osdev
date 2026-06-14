#pragma once
#include <kernel.h>

KSTATUS VmmInitalize();
void* VmmAllocate(uint64_t size);
void VmmFree(void* ptr);