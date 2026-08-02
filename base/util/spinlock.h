#pragma once

#include <stdatomic.h>
#include <stdint.h>
typedef struct {
    atomic_flag x; // stores the actual lock
    uint64_t rfl; // stores old rfl
} Spinlock;

void SpnLckAcquire(Spinlock* lock);
void SpnLckRelease(Spinlock* lock);
uint64_t SpnLckAcquireRfl(Spinlock* lock);
void SpnLckReleaseRfl(Spinlock* lock, uint64_t rfl);