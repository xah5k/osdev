#include <arch/x86_64/cpu/paging.h>
#include <arch/x86_64/cpu/cpu.h>
#include <arch/x86_64/cpu/idt.h>
#include <printfwrapper.h>
#include <stdint.h>
#include "lapic.h"
#include <sched/sched.h>
#include <arch/x86_64/ports.h>
#include <kedriver.h>


physaddr CpuGetLapicPhysicalBase() {
    return (CpuReadMsr(IA32_APIC_BASE) & 0x000FFFFFFFFFF000);
}
physaddr lapicphysbase;
virtaddr lapicvirtbase;
uint64_t lapictimertick = 0;

#define LAPIC_SPURIOUS_REG 0x00F0
#define LAPIC_EOI_REG 0x00B0
void CpuEnableLapic() {
    uint32_t spuriousreg = *(volatile uint32_t*)(lapicvirtbase + LAPIC_SPURIOUS_REG);

    spuriousreg |= (1 << 8);
    spuriousreg |= 0xFF;

    *(volatile uint32_t*)(lapicvirtbase + LAPIC_SPURIOUS_REG) = spuriousreg;
}

void CpuLapicTimerHandler(CpuInterruptArgs* r) {
    lapictimertick++;
    CpuLapicEoi();
    Schedule();
}

uint64_t gCpuLapicTicksPer10ms = 0;
uint64_t gCpuLapicTicksPerMs = 0;

uint64_t CpuLapticTimerGetTick() {
    return lapictimertick;
}

void CpuCalibrateLapicTimer() {
    asm ("cli");
    // initalize pit
    outb(0x43, 0x30);
    outb(0x40, 0xFF);
    outb(0x40, 0xFF);
    *(volatile uint32_t*)(lapicvirtbase + 0x3E0) = 0x03;
    *(volatile uint32_t*)(lapicvirtbase + 0x380) = 0xFFFFFFFF;
    outb(0x43, 0x00);
    uint8_t low  = inb(0x40);
    uint8_t high = inb(0x40);
    uint16_t start_val = low | (high << 8);
    uint16_t current_val = 0;
    uint16_t elapsed = 0;
    while (!(elapsed >= 11932)) {
        uint8_t low  = inb(0x40);
        uint8_t high = inb(0x40);
        current_val = low | (high << 8);
        elapsed = start_val - current_val;
    }
    // get lapic current count
    uint32_t lapic_current = *(volatile uint32_t*)(lapicvirtbase + 0x390);
    gCpuLapicTicksPer10ms = 0xFFFFFFFF - (uint64_t)lapic_current;
    gCpuLapicTicksPerMs = gCpuLapicTicksPer10ms / 10;
    asm ("sti");
    printf("lapic: timer ticks %d times per 10ms\r\n", gCpuLapicTicksPer10ms);
}
// initalize and calibrate it
void CpuInitalizeLapicTimer(uint64_t ivector) {
    CpuCalibrateLapicTimer();
    *(volatile uint32_t*)(lapicvirtbase + 0x380) = gCpuLapicTicksPer10ms;
    *(volatile uint32_t*)(lapicvirtbase + 0x320) = 0x20000 | ivector;
    CpuRegisterHandler(ivector, (irqhandler)CpuLapicTimerHandler);
}

void CpuLapicEoi() {
    *(volatile uint32_t*)(lapicvirtbase + LAPIC_EOI_REG) = 0;
}
KE_EXPORT_SYMBOL(CpuLapicEoi);

uint32_t CpuLapicGetId() {
    uint32_t id = *(volatile uint32_t*)(lapicvirtbase + 0x0020);
    return id;
}

void CpuInitalizeLapic() {
    uint64_t efer = CpuReadMsr(IA32_EFER);
    efer |= (1 << 11);
    CpuWriteMsr(0xC0000080, efer);

    lapicphysbase = CpuGetLapicPhysicalBase();
    lapicvirtbase = (virtaddr)(lapicphysbase + gMmuVOffset);
    // XD isn't currently possible due to how BOOTBOOT expects a kernel binary to be
    MmuMapPage((pagetable*)_x86_64_get_pml4(), lapicvirtbase, lapicphysbase, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_PWT | MMU_PAGE_BIT_PCD);
    CpuEnableLapic();
}