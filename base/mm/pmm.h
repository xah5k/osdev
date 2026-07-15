#pragma once
#include <stdint.h>
#include "../external/bootboot.h"
#include <kernel.h>
KSTATUS PmmInitalize(BOOTBOOT* b);

void* PmmAllocatePages(uint64_t num);
void* PmmAllocate();
void PmmFree(void* page);
void PmmFreePages(void* pagef, uint64_t num);
void PmmAdjustBitmapPtr(); // before we get rid of identity map