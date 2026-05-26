#pragma once
#include <stdint.h>
#include "../external/bootboot.h"
void PmmInitalize(BOOTBOOT* b);

void* PmmAllocate();
void PmmFree(void* page);