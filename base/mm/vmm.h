#pragma once
#include <kernel.h>

KSTATUS VmmInitalize();
void* VmmAllocate(uint64_t size);
void VmmFree(void* ptr);
void* VmmAllocateAt(uint64_t size, uint64_t cr3);
void VmmFreeAt(void* ptr, uint64_t cr3);