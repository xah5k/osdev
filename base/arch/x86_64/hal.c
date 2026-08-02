#include <arch/x86_64/hal.h>
#include <mm/heap.h>
#include <arch/x86_64/cpu/lapic.h>
#include <arch/x86_64/cpu/ioapic.h>
#include <stddef.h>
#include <external/printf.h>
#include <memory.h>
#include <arch/x86_64/cpu/gdt.h>
#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/cpu/lapic.h>
#include "arch/x86_64/cpu/ioapic.h"
#include <arch/x86_64/acpi.h>
#include <arch/x86_64/pci/pci.h>
void HalInitalize(KernelInformation* kinfo) {
    CpuInitalizeGdt((struct KernelInformation*)kinfo);
    CpuInitalizeIdt();
    asm ("sti");
    CpuInitalizeLapic();
    CpuInitalizeIoApic(kinfo->rsdt);
	CpuInitalizeLapicTimer(32);
    kinfo->cpufeats = CpuDetectFeatures();
    AcpiMcfgTable* table = (AcpiMcfgTable*)AcpiFindTable(kinfo->rsdt, "MCFG");
    PciEnumerate(table);
}
