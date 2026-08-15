#pragma once

#include "cpu.h"
typedef void (*irqhandler)(CpuInterruptArgs* r);

void CpuInitalizeIdt();
void CpuRegisterHandler(uint64_t index, irqhandler handler);
void CpuRegisterHandlerArg(uint64_t index, void* arg);