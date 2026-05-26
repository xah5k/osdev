#pragma once
#include <stdint.h>

extern void _x86_64_load_gdt(uint64_t);

void CpuInitalizeGdt();