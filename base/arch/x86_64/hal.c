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
#include <arch/x86_64/uacpi_compat.h>
#include "serial.h"
#include <mm/pmm.h>

KSTATUS HalInitalize(KernelInformation* kinfo) {
    CpuInitalizeGdt((struct KernelInformation*)kinfo);
    CpuInitalizeIdt();
    asm ("sti");
    CpuInitalizeLapic();
    CpuInitalizeIoApic(kinfo->rsdt);
	CpuInitalizeLapicTimer(32);
    kinfo->cpufeats = CpuDetectFeatures();
    AcpiMcfgTable* table = (AcpiMcfgTable*)AcpiFindTable(kinfo->rsdt, "MCFG");
    PciEnumerate(table);
    KATTEMPT(uAcpiInitalize() == KSUCCESS);
    KATTEMPT(AcpiResolvePciGsi() == KSUCCESS);
    return KSUCCESS;
}

KSTATUS HalInitalizeSerial() {
    InitSerialConsole(0x3f8);
    return KSUCCESS;
}
KSTATUS HalPutChar(char c) {
    WritecSerial(0x3f8, c);
    return KSUCCESS;
}
static pagetable* kpml4;
extern uint64_t PmmTotalPhysicalMem;
extern uint8_t _kernel_start;
extern uint8_t _kernel_end;
extern BOOTBOOT bootboot;

KSTATUS HalSetupMmu(KernelInformation* gkInfo) {
    kpml4 = PmmAllocate();
    memset(kpml4, 0, PAGE_SIZE);

    // map every usable page
    for (uint64_t i = 0; i < PmmTotalPhysicalMem; i+=PAGE_SIZE) {
        MmuMapPage(kpml4, i + MMU_PHYS_OFFSET, i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
        // tmp identity map
        MmuMapPage(kpml4, i, i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    } 

    // physical address of kernel
    uint64_t kphys = MmuGetPhys(_x86_64_get_pml4(), 0xffffffffffe02000);
    uint64_t ksize = (uint64_t)&_kernel_end - (uint64_t)&_kernel_start;
    // map kernel
    for (uint64_t i = 0; i < ksize; i+=PAGE_SIZE) {
        MmuMapPage(kpml4, 0xffffffffffe02000 + i, kphys + i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }

    // map framebuffer
    for (uint64_t i = 0; i < bootboot.fb_size; i+=PAGE_SIZE) {
        MmuMapPage(kpml4, 0xfffffffffc000000 + i, bootboot.fb_ptr + i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }

    // map bootboot struct
    MmuMapPage(kpml4, 0xffffffffffe00000, MmuGetPhys(_x86_64_get_pml4(), 0xffffffffffe00000), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);

    // map initrd
    for (uint64_t i = bootboot.initrd_ptr; i < bootboot.initrd_size; i+=PAGE_SIZE) {
        //printf("paging: initrd: mapped phys(0x%lx) to virt(0x%lx)\r\n", i, (i + gMmuVOffset));
        MmuMapPage(kpml4, (i + MMU_PHYS_OFFSET), (i), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }


    // map kernel info struct
    for (uint64_t i = (uint64_t)gkInfo; i < sizeof(KernelInformation); i+=PAGE_SIZE) {
        MmuMapPage(kpml4, (i + MMU_PHYS_OFFSET), i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }

    // map stack
    MmuMapPage(kpml4, _x86_64_get_stack(), MmuGetPhys(_x86_64_get_pml4(), _x86_64_get_stack()), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    //_x86_64_set_stack(_x86_64_get_stack() + MMU_PHYS_OFFSET);

    // map pml4 itself
    MmuMapPage(kpml4, (virtaddr)((uint64_t)kpml4 + MMU_PHYS_OFFSET), (physaddr)kpml4, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);

    // switch
    _x86_64_load_pml4((uint64_t)kpml4);
    
    // change offset (where physical memory is located in virtual address space)
    gMmuVOffset = MMU_PHYS_OFFSET;
    return KSUCCESS;
}

KSTATUS HalRmvIdentityMap() {
    kpml4[0] = 0;
    _x86_64_load_pml4((uint64_t)kpml4);
    return KSUCCESS;
}

pagetable* HalGetPageTable() {
    return kpml4;
}

void HalUserJump(uint64_t entry, uint64_t usersp, uint64_t userargv, uint64_t userargc) {
    _x86_64_usjmp(entry, usersp, userargv, userargc);
}
uint64_t HalGetStack() {
    return _x86_64_get_stack();
}

void HalContextSw(uint64_t* old, uint64_t new) {
    _x86_64_ctxswitch(old, new);
}

void HalSwPageTable(uint64_t new) {
    _x86_64_load_pml4(new);
}