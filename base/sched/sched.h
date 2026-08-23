#pragma once
#include "kernel.h"
#include <stdint.h>
#include "process.h"

void SchedInitalize(KernelInformation* kinfo);
void ThreadCreate(ThreadCtrlBlk* Tcb, void* entry);
void Schedule();
void SchedYield();