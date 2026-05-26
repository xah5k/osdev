#include <stdint.h>
#include <memory.h>
#include <external/bootboot.h>
#include <printfwrapper.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include "fs/vfs.h"
#include "sched/process.h"
#include "serial.h"
#include <kernel.h>
#ifdef __x86_64__
#include <arch/x86_64/cpu/cpu.h>
#include <arch/x86_64/cpu/paging.h>
#include <arch/x86_64/cpu/gdt.h>
#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/cpu/lapic.h>
#include "arch/x86_64/kbd.h"
#include "arch/x86_64/cpu/ioapic.h"
#include "arch/x86_64/hal.h"
#endif
#include <fs/tar.h>
#include <sched/sched.h>


extern BOOTBOOT bootboot;               // see bootboot.h
extern unsigned char environment[4096]; // configuration, UTF-8 text key=value pairs
extern uint8_t fb;                      // linear framebuffer mapped

extern uint8_t _kernel_start;
extern uint8_t _kernel_end;

//uint8_t kstack[16384];

void _putchar(char character) {
    WritecSerial(0x3f8, character);
    #ifdef _BUILD_FB_CON
    FbPutc(character);
    #endif
}


const char* BugcheckTable[2] = {
    "UNREGISTERED_INTERRUPT",
    "INTENTIONAL_INVOCATION",
};

void KernelBugcheck(BugcheckCode code, CpuInterruptArgs* registers) {
    printf("kernel: unrecoverable bugcheck\r\n");
    printf("kernel: bugcheck type: %s [0x%x]\r\n", BugcheckTable[code], code);
    if (registers) {
        printf("kernel: interrupt frame dump: \r\n");
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
        printf("kernel: end dump\r\n");
    } else {
        printf("kernel: no interrupt frame provided (non interrupt?)\r\n");
    }
    printf("kernel: halting\r\n");
    while (1) {
        asm ("cli; hlt");
    }
}

void KernelBootstrapProc();
void KernelApplicationProc();

extern uint64_t PmmTotalPhysicalMem;

pagetable* kpml4; 

static KernelInformation* gkInfo;

struct ProcessCtrlBlk* KernelGetCurrentProc() {
    return gkInfo->CurrentProcess;
}

void KernelSetupPaging() {
    kpml4 = PmmAllocate();
    memset(kpml4, 0, PAGE_SIZE);
    #ifdef __x86_64__
    printf("paging: current pml4 = 0x%lx\r\n", _x86_64_get_pml4());
    #endif
    // map every usable page
    for (uint64_t i = 0; i < PmmTotalPhysicalMem; i+=PAGE_SIZE) {
        MmuMapPage(kpml4, i + MMU_PHYS_OFFSET, i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
        // tmp identity map
        MmuMapPage(kpml4, i, i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    } 

    printf("paging: mapped usable pages\r\n");
    // physical address of kernel
    uint64_t kphys = MmuGetPhys(0xffffffffffe02000);
    uint64_t ksize = (uint64_t)&_kernel_end - (uint64_t)&_kernel_start;
    printf("paging: physical address of kernel = 0x%lx\r\n", kphys);
    printf("paging: size of kernel: %d\r\n", ksize);

    // map kernel
    for (uint64_t i = 0; i < ksize; i+=PAGE_SIZE) {
        MmuMapPage(kpml4, 0xffffffffffe02000 + i, kphys + i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }
    printf("paging: mapped kernel pages\r\n");

    // map framebuffer
    for (uint64_t i = 0; i < bootboot.fb_size; i+=PAGE_SIZE) {
        MmuMapPage(kpml4, 0xfffffffffc000000 + i, bootboot.fb_ptr + i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }

    printf("paging: mapped framebuffer pages\r\n");
    // map bootboot struct
    MmuMapPage(kpml4, 0xffffffffffe00000, MmuGetPhys(0xffffffffffe00000), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    printf("paging: mapped bootloader info\r\n");

    // map initrd
    for (uint64_t i = bootboot.initrd_ptr; i < bootboot.initrd_size; i+=PAGE_SIZE) {
        //printf("paging: initrd: mapped phys(0x%lx) to virt(0x%lx)\r\n", i, (i + gMmuVOffset));
        MmuMapPage(kpml4, (i + MMU_PHYS_OFFSET), (i), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }

    printf("paging: mapped initrd\r\n");

    // map kernel info struct
    for (uint64_t i = (uint64_t)gkInfo; i < sizeof(KernelInformation); i+=PAGE_SIZE) {
        MmuMapPage(kpml4, (i + MMU_PHYS_OFFSET), i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    }
    printf("paging: mapped kernel info structure\r\n");

    // map stack
    #ifdef __x86_64__
    MmuMapPage(kpml4, _x86_64_get_stack(), MmuGetPhys(_x86_64_get_stack()), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    //_x86_64_set_stack(_x86_64_get_stack() + MMU_PHYS_OFFSET);
    printf("paging: mapped stack\r\n");
    #endif

    // map pml4 itself
    #ifdef __x86_64__
    MmuMapPage(kpml4, (virtaddr)((uint64_t)kpml4 + MMU_PHYS_OFFSET), (physaddr)kpml4, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    printf("paging: remapped pml4 into higher half\r\n");
    #endif

    // switch
    #ifdef __x86_64__
    _x86_64_load_pml4((uint64_t)kpml4);
    #endif

    // change offset (where physical memory is located in virtual address space)
    gMmuVOffset = MMU_PHYS_OFFSET;


    printf("paging: switched page tables!\r\n");
    #ifdef __x86_64__
    printf("paging: current pml4 (hw): 0x%lx expected pml4: 0x%lx\r\n", _x86_64_get_pml4(), kpml4);
    #endif
}

void KernelBootstrapProc() {

    // initalize serial console (bootboot in theory should've already done this for us)
    InitSerialConsole(0x3f8);

    WriteSerial(0x3f8, "wsp gngos\r\n");

    PmmInitalize(&bootboot);
    printf("kernel: finished initalizing pmm.\r\n");

    gkInfo = (KernelInformation*)PmmAllocate();
    //gkInfo->pml4 = kpml4;
    gkInfo->bootinfo = &bootboot;
    gkInfo->initrd = (void*)(bootboot.initrd_ptr);
    gkInfo->fb = (Framebuffer*)PmmAllocate();
    gkInfo->fb->ptr = 0xfffffffffc000000;
    gkInfo->fb->size = bootboot.fb_size;
    gkInfo->fb->width = bootboot.fb_width;
    gkInfo->fb->height = bootboot.fb_height;
    gkInfo->fb->scanline = bootboot.fb_scanline;

#ifdef _BUILD_FB_CON
    TarFileEntry* sfnentry = TarFsLookup(gkInfo->initrd, "boot/font.psf");
    char* sfnraw = (char*)(sfnentry + 1);
    printf("kernel: font file entry @ 0x%lx raw data should start @ 0x%lx\r\n", sfnentry, sfnraw);
    FbTextInitalize((void*)sfnraw, (void*)gkInfo->fb);
    printf("kernel: finished initalizing framebuffer and loading in kernel console font\r\n");
#endif
    KernelSetupPaging();
    MmuMapPage(kpml4, (virtaddr)((uint64_t)gkInfo->fb + MMU_PHYS_OFFSET), (physaddr)gkInfo->fb, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    gkInfo = (KernelInformation*)((uint64_t)gkInfo + MMU_PHYS_OFFSET);
    gkInfo->fb = (Framebuffer*)((uint64_t)gkInfo->fb + MMU_PHYS_OFFSET);
    printf("kernel: gkInfo changed address to virt(0x%lx)\r\n", gkInfo);

    printf("kernel: finished initalizing new page tables.\r\n");
    //printf("gkinfo->pml4 = 0x%lx kpml4 = 0x%lx\r\n", gkInfo->pml4, kpml4);
    VmmInitalize();

    printf("kernel: finished initalizing vmm.\r\n");
    
    MmHeapInitalize();
    printf("kernel: finished intializing kernel heap.\r\n");

    SchedInitalize(gkInfo);
    printf("kernel: initalized scheduler structures.\r\n");
    //printf("kernel: gkInfo->CurrentProcess = 0x%p gkInfo->ProcessListHead = 0x%p\r\n", gkInfo->CurrentProcess, gkInfo->ProcessListHead);
    #ifdef __x86_64__
    // note we are initalizing the gdt really late
    CpuInitalizeGdt();
    printf("kernel: finished initalizing memory segmentation\r\n");

    CpuInitalizeIdt();
    printf("kernel: loaded new idt in\r\n");
    asm ("sti");

    CpuInitalizeLapic();
    printf("kernel: finished enabling lapic and lapic timer!\r\n");
    //printf("kernel: current lapic tick: %d\r\n", CpuLapticTimerGetTick());
    gkInfo->rsdt = (void*)((uint64_t)bootboot.arch.x86_64.acpi_ptr + gMmuVOffset);
    //printf("kernel: rsdt@0x%lx\r\n", gkInfo->rsdt);

    CpuInitalizeIoApic(gkInfo->rsdt);
    printf("kernel: finished enabling ioapic\r\n");

	HalInitalize();    
#endif
    gkInfo->initrd = (void*)(bootboot.initrd_ptr + MMU_PHYS_OFFSET);
    ProcListRunning(gkInfo);

    TarInitalizeVfs(gkInfo->initrd);
    printf("kernel: initalized tarfs\r\n");
    while(1);
}

void KernelApplicationProc() {
    while(1) { __asm__("cli; hlt"); }
}
