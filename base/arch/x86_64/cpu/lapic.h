#pragma once
#include "cpu.h"
//#include "idt.h"
void CpuLapicEoi();
uint64_t CpuLapticTimerGetTick();
void CpuLapicTimerHandler(CpuInterruptArgs* r);
void CpuInitalizeLapic();
uint32_t CpuLapicGetId();
void CpuInitalizeLapicTimer(uint64_t ivector);