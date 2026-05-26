#pragma once
#include "kernel.h"
#include <stdint.h>

#ifdef __x86_64__
#include <arch/x86_64/cpu/cpu.h>
#endif
#include "process.h"

#define SCHED_THREAD_READY 1
#define SCHED_THREAD_RUNNING 2




void SchedInitalize(KernelInformation* kinfo);
void ThreadCreate(ThreadCtrlBlk* Tcb, void* entry);
void Schedule();
void SchedYield();