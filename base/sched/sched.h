#pragma once
#include "kernel.h"
#include <stdint.h>
#include "process.h"
typedef struct KeSchedQueue {
    ThreadCtrlBlk* Head;
    ThreadCtrlBlk* Tail;
} KeSchedQueue;

typedef struct KeScheduler {
    KeSchedQueue Queues[SCHED_THREAD_PHIGH];
    uint64_t Bitmap;
} KeScheduler;
void SchedInitalize(KernelInformation* kinfo);
void ThreadCreate(ThreadCtrlBlk* Tcb, void* entry);
void Schedule(KeScheduler* Sched);
void SchedYield();