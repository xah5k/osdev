#include "util/spinlock.h"
#include <printfwrapper.h>
#include <stdatomic.h>
#include <stdint.h>
#include <arch/x86_64/cpu/paging.h>
#include <arch/x86_64/acpi.h>
#include <arch/x86_64/cpu/ioapic.h>


#define IOAPIC_SELECTOR 0x00
#define IOAPIC_WINDOW 0x10
#define IOAPIC_REDIRENTRYOFFSET 0x10
physaddr ioapicphysbase;
virtaddr ioapicvirtbase;
static Spinlock IoApicLock = {ATOMIC_FLAG_INIT};

void CpuIoApicWrite(uint32_t reg, uint32_t value) {
    SpnLckAcquire(&IoApicLock);
    *(volatile uint32_t*)(ioapicvirtbase + IOAPIC_SELECTOR) = reg;
    *(volatile uint32_t*)(ioapicvirtbase + IOAPIC_WINDOW) = value;
    SpnLckRelease(&IoApicLock);
}

uint32_t CpuIoApicRead(uint32_t reg) {
    SpnLckAcquire(&IoApicLock);
    *(volatile uint32_t*)(ioapicvirtbase + IOAPIC_SELECTOR) = reg;
    SpnLckRelease(&IoApicLock);
    return *(volatile uint32_t*)(ioapicvirtbase + IOAPIC_WINDOW);
}

virtaddr CpuGetIoApicVirtBase() {
    return ioapicvirtbase;
}

physaddr CpuGetIoApicPhysBase(AcpiMadtTable* madt) {
    AcpiMadtIntDeviceHdr* Hdr = madt->IntDevices;
    uint64_t end = ((uint64_t)Hdr + madt->header.Length);
    while ((uint64_t)Hdr < end) {
        if (Hdr->Type == 1) {
            AcpiMadtIntDevIoApic* DevIoApic = (AcpiMadtIntDevIoApic*)Hdr;
            printf("ioapic: found ioapic entry with id %d and length %d\r\n", DevIoApic->IoApicId, DevIoApic->header.Length);
            printf("ioapic: with ioapic physaddr @ 0x%lx\r\n", DevIoApic->IoApicAddress);
            printf("ioapic: with ioapic gsi @ 0x%lx\r\n", DevIoApic->GsiBase);
            return DevIoApic->IoApicAddress;
        }
        //printf("ioapic: hdr @ 0x%lx end @ 0x%lx\r\n", Hdr, end);
        Hdr = (AcpiMadtIntDeviceHdr*)((uint8_t*)Hdr + Hdr->Length);
    }
    return 0;
}

void CpuIoApicSetRedirEntry(uint8_t gsi, uint64_t data) {
    uint8_t low = IOAPIC_REDIRENTRYOFFSET + (gsi * 2);
    uint8_t high = low + 1;

    CpuIoApicWrite(low, (uint32_t)data);
    CpuIoApicWrite(high, (uint32_t)(data >> 32));
}

void CpuInitalizeIoApic(AcpiRsdtTable* rsdt) {
    AcpiMadtTable* madt = AcpiFindTable(rsdt, "APIC");
    printf("ioapic: madt@0x%lx\r\n", madt);
    printf("ioapic: lapic physical address (madt->LapicAddress): 0x%lx\r\n", madt->LapicAddress);   
    ioapicphysbase = CpuGetIoApicPhysBase(madt);
    ioapicvirtbase = (ioapicphysbase + gMmuVOffset);
    printf("ioapic: ioapic phys base: 0x%lx ioapic virt base: 0x%lx\r\n", ioapicphysbase, ioapicvirtbase);
    MmuMapPage((pagetable*)_x86_64_get_pml4(), ioapicvirtbase, ioapicphysbase, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE | MMU_PAGE_BIT_PWT | MMU_PAGE_BIT_PCD);
    printf("ioapic: mapped phys to virtual\r\n");
    uint32_t verreg = CpuIoApicRead(0x01);
    uint32_t maxredirentry = (verreg & 0xFF0000) >> 16;
    for (int i = 0; i <= maxredirentry; i++) {
        uint32_t low = CpuIoApicRead(IOAPIC_REDIRENTRYOFFSET + (i * 2));
        low |= (1<<16);
        CpuIoApicWrite(IOAPIC_REDIRENTRYOFFSET + (i * 2), low);
    }
    printf("ioapic: masked every pin\r\n");
}