#pragma once
#include <kernel.h>

void VmmInitalize();
void* VmmAllocate(uint64_t size);
void VmmFree(void* ptr);