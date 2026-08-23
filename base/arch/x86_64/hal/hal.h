#pragma once
#include <stdint.h>
#include <kernel.h>
#include "../cpu/paging.h"

void HalUserJump(uint64_t entry, uint64_t usersp, uint64_t userargv, uint64_t userargc);
uint64_t HalGetStack();
void HalContextSw(uint64_t* old, uint64_t new);