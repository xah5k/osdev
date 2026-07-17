#include "arch/x86_64/cpu/cpu.h"
#include <stdatomic.h>
#include <util/spinlock.h>
#include <kedriver.h>
void SpnLckAcquire(Spinlock* lock) {

    // save rfl
    #ifdef __x86_64__
    uint64_t rfl;
    asm volatile ("pushfq; pop %0" : "=r"(rfl));
    asm volatile ("cli");
    #endif
    while (atomic_flag_test_and_set_explicit(&lock->x, memory_order_acquire)) {
        #ifdef __x86_64__
        _x86_64_pause();
        #endif
    }
    #ifdef __x86_64__
    lock->rfl = rfl;
    #endif
}
KE_EXPORT_SYMBOL(SpnLckAcquire);
void SpnLckRelease(Spinlock* lock) {
    #ifdef __x86_64__
    uint64_t rfl = lock->rfl;
    #endif
    atomic_flag_clear_explicit(&lock->x, memory_order_release);
    #ifdef __x86_64__
    asm volatile("push %0; popfq" : : "r"(rfl));
    #endif
}
KE_EXPORT_SYMBOL(SpnLckRelease);