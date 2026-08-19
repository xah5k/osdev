#include <kernel.h>
#include <kedriver.h>
#include <fb.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <arch/x86_64/cpu/idt.h>

void IntVector1Handler(CpuInterruptArgs* r) {
    KeDrvWrite("ivector1: debug interrupt\r\n");
    __asm__ volatile("mov %0, %%dr6" :: "r"((uint64_t)0));
    r->rflags &= ~(1ULL << 8);
}

KSTATUS DriverEntry(KeDriverObj* Self) {
    CpuRegisterHandler(1, (irqhandler)IntVector1Handler);
    return KSUCCESS;
}