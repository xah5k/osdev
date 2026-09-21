#pragma once
#include "signal.h"
#include <util/spinlock.h>

typedef struct KeMessageObj {
    uint64_t FromPid;
    uint64_t ToPid;
    uint64_t Length;
    struct KeMessageObj* Next;
} KeMessageObj;

KSTATUS KeMessageSend(KeMessageObj* Obj);
KeMessageObj* KeMessagePopHead(KeMessageObj** Head, KeMessageObj** Tail, uint64_t* MCount, Spinlock* QueueLock);