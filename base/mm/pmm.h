#pragma once
#include <stdint.h>
#include <external/limine.h>
#include <kernel.h>
KSTATUS PmmInitalize(struct limine_memmap_response* resp, uint64_t hhdm);

void* PmmAllocatePages(uint64_t num);
void* PmmAllocate();
void PmmFree(void* page);
void PmmFreePages(void* pagef, uint64_t num);
void PmmAdjustBitmapPtr(); // before we get rid of identity map