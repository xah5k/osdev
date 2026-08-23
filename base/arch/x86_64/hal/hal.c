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
#include "../serial.h"
#include <mm/pmm.h>
#include <sched/process.h>
#include <kedriver.h>

KSTATUS HalInitalize(KernelInformation* kinfo) {
    KeDriverObj* driver = MmAllocate(sizeof(KeDriverObj));
    memcpy(driver->Name, "hal-amd64", 10);
    KeDrvRegisterDriver(driver);
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
extern uint64_t PmmHighestAddr;
extern uint8_t _kernel_start;
extern uint8_t _kernel_end;
extern uint64_t kstack_bottom;
extern uint64_t kstack_top;
KSTATUS HalSetupMmu(KernelInformation* gkInfo) {
    kpml4 = PmmAllocate();
    memset((void*)((uint64_t)kpml4 + gMmuVOffset), 0, PAGE_SIZE);

    // map every usable page
    for (uint64_t i = 0; i < PmmHighestAddr; i+=PAGE_SIZE) {
        MmuMapPage((pagetable*)P2V(kpml4), i + gMmuVOffset, i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
        // tmp identity map
        MmuMapPage((pagetable*)P2V(kpml4), i, i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    } 

    // map kernel info struct
    for (uint64_t i = (uint64_t)V2P(gkInfo); i < sizeof(KernelInformation); i+=PAGE_SIZE) {
        MmuMapPage((pagetable*)P2V(kpml4), (i + gMmuVOffset), i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }
    // physical address of kernel
    uint64_t kphys = MmuGetPhys(_x86_64_get_pml4(), 0xffffffffffe02000);
    uint64_t ksize = (uint64_t)&_kernel_end - (uint64_t)&_kernel_start;
    // map kernel
    for (uint64_t i = 0; i < ksize; i+=PAGE_SIZE) {
        MmuMapPage((pagetable*)P2V(kpml4), 0xffffffffffe02000 + i, kphys + i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }
    // map stack
    for (uint64_t i = (uint64_t)&kstack_bottom; i < (uint64_t)&kstack_top; i+=PAGE_SIZE) {
        MmuMapPage((pagetable*)P2V(kpml4), i, MmuGetPhys(_x86_64_get_pml4(), i), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }
    // map fb
    for (uint64_t i = 0; i < gkInfo->fb->size; i++) {
        MmuMapPage((pagetable*)P2V(kpml4), (i + gkInfo->fb->ptr), V2P(gkInfo->fb->ptr + i), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }
    //_x86_64_set_stack(_x86_64_get_stack() + gMmuVOffset);
    // map pml4 itself
    MmuMapPage((pagetable*)P2V(kpml4), (virtaddr)((uint64_t)kpml4 + gMmuVOffset), (physaddr)kpml4, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    // switch
    _x86_64_load_pml4((uint64_t)kpml4);
    // change offset (where physical memory is located in virtual address space)
    //gMmuVOffset = gMmuVOffset;
    return KSUCCESS;
}

KSTATUS HalRmvIdentityMap() {
    pagetable* vpml4 = (pagetable*)P2V(kpml4);
    vpml4[0] = 0;
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
void HalDumpRegisters(CpuInterruptArgs* registers) {
    printf("hal: interrupt frame dump: \r\n");
    printf("   InterruptVector: %d   ErrorCode: 0x%lx\r\n", registers->intnum, registers->errcode);
    printf("   rip: 0x%016lx            cs: 0x%016lx\r\n", registers->rip, registers->cs);
    printf("   rfl: 0x%016lx         rsp: 0x%016lx\r\n", registers->rflags, registers->rsp);

    printf("   r15: 0x%016lx            r14: 0x%016lx\r\n", registers->r15, registers->r14);
    printf("   r13: 0x%016lx            r12: 0x%016lx\r\n", registers->r13, registers->r12);
    printf("   r11: 0x%016lx            r10: 0x%016lx\r\n", registers->r11, registers->r10);
    printf("   r9: 0x%016lx             r8: 0x%016lx\r\n", registers->r9, registers->r8);
    printf("   rdi: 0x%016lx            rsi: 0x%016lx\r\n", registers->rdi, registers->rsi);
    printf("   rdx: 0x%016lx            rcx: 0x%016lx\r\n", registers->rdx, registers->rcx);
    printf("   rbx: 0x%016lx            rax: 0x%016lx\r\n", registers->rbx, registers->rax);
    printf("   rbp: 0x%016lx            ss: 0x%016lx\r\n", registers->rbp, registers->ss);
    printf("hal: end dump\r\n");
}

void HalReloadCr3() {
    asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
}

void HalContextSwPrep(struct ThreadCtrlBlk* NextThr) {
    KernelGetInformation()->tss->rsp0 = NextThr->KernelRsp;
    CpuWriteMsr(0xC0000100, NextThr->FsBase);
    CpuWriteMsr(0xC0000102, NextThr->GsBase);
}