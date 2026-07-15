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
#include <util/spinlock.h>
#include <kernel.h>
#include <exeldr/ldrelf.h>
#ifdef __x86_64__
#include <arch/x86_64/cpu/cpu.h>
#include <arch/x86_64/cpu/paging.h>
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
}


const char* BugcheckTable[3] = {
    "UNREGISTERED_INTERRUPT",
    "INTENTIONAL_INVOCATION",
    "KERNEL_CORE_COMP_FAIL"
};

void KdBugcheck(BugcheckCode code, CpuInterruptArgs* registers) {
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

void KdBugcheck2(BugcheckCode code, CpuInterruptArgs* registers, int line, char* filename) {
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
    if (line && filename) {
        printf("kernel: file and line: %s:%d\r\n", filename, line);
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
Spinlock KernelResourceLock = {ATOMIC_FLAG_INIT};

void KernelUnlockRsLck() {
    SpnLckRelease(&KernelResourceLock);
}
struct ProcessCtrlBlk* KernelGetCurrentProc() {
    SpnLckAcquire(&KernelResourceLock);
    return gkInfo->CurrentProcess;
}

KernelInformation* KernelGetInformation() {
    return gkInfo;
}

void KeSetupMmu() {
    kpml4 = PmmAllocate();
    memset(kpml4, 0, PAGE_SIZE);

    // map every usable page
    for (uint64_t i = 0; i < PmmTotalPhysicalMem; i+=PAGE_SIZE) {
        MmuMapPage(kpml4, i + MMU_PHYS_OFFSET, i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
        // tmp identity map
        MmuMapPage(kpml4, i, i, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    } 

    // physical address of kernel
    uint64_t kphys = MmuGetPhys(0xffffffffffe02000);
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
    MmuMapPage(kpml4, 0xffffffffffe00000, MmuGetPhys(0xffffffffffe00000), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);

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
    #ifdef __x86_64__
    MmuMapPage(kpml4, _x86_64_get_stack(), MmuGetPhys(_x86_64_get_stack()), MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    //_x86_64_set_stack(_x86_64_get_stack() + MMU_PHYS_OFFSET);
    #endif

    // map pml4 itself
    #ifdef __x86_64__
    MmuMapPage(kpml4, (virtaddr)((uint64_t)kpml4 + MMU_PHYS_OFFSET), (physaddr)kpml4, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    #endif

    // switch
    #ifdef __x86_64__
    _x86_64_load_pml4((uint64_t)kpml4);
    #endif
    
    // change offset (where physical memory is located in virtual address space)
    gMmuVOffset = MMU_PHYS_OFFSET;
}

void KeRmvIdentityMap() {
    for (uint64_t i = 0; i < PmmTotalPhysicalMem; i+=PAGE_SIZE) {
        // tmp identity map
        MmuUnmapPage((pagetable*)((uint64_t)kpml4 + MMU_PHYS_OFFSET), i);
    } 
}
KSTATUS KeMmInitalize() {
    return VmmInitalize() && MmHeapInitalize();
}

static KernelInformation* KeCreateKinfo() {
    KernelInformation* kInfo = PmmAllocate();
    kInfo->bootinfo = &bootboot;
    kInfo->initrd = (void*)(bootboot.initrd_ptr);
    kInfo->fb = (Framebuffer*)PmmAllocate();
    kInfo->fb->ptr = 0xfffffffffc000000;
    kInfo->fb->size = bootboot.fb_size;
    kInfo->fb->width = bootboot.fb_width;
    kInfo->fb->height = bootboot.fb_height;
    kInfo->fb->scanline = bootboot.fb_scanline;
    return kInfo;
}

extern uint64_t PmmLargestFreeMemorySize;

void KernelBootstrapProc() {
    // initalize serial console (bootboot in theory should've already done this for us)
    InitSerialConsole(0x3f8);

    // setup pmm
    PmmInitalize(&bootboot);
    printf("kernel: finished initalizing pmm.\r\n");

    // create kernel information structure
    gkInfo = KeCreateKinfo();
    KATTEMPT(gkInfo);

    // setup paging
    KeSetupMmu();
    MmuMapPage(kpml4, (virtaddr)((uint64_t)gkInfo->fb + MMU_PHYS_OFFSET), (physaddr)gkInfo->fb, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    gkInfo = (KernelInformation*)((uint64_t)gkInfo + MMU_PHYS_OFFSET);
    gkInfo->fb = (Framebuffer*)((uint64_t)gkInfo->fb + MMU_PHYS_OFFSET);
    gkInfo->rsdt = (void*)((uint64_t)bootboot.arch.x86_64.acpi_ptr + gMmuVOffset);
    printf("kernel: finished initalizing new page tables.\r\n");
    
    // setup higher memory management
    KeMmInitalize();
    printf("kernel: initalized higher mm.\r\n");

    // setup sched structures
    SchedInitalize(gkInfo);
    printf("kernel: initalized scheduler structures.\r\n");

    #ifdef __x86_64__
	HalInitalize(gkInfo);    
    printf("kernel: initalized hal for arch x86-64!\r\n");
    #endif

    gkInfo->initrd = (void*)(bootboot.initrd_ptr + MMU_PHYS_OFFSET);
    TarInitalizeVfs(gkInfo->initrd);
    printf("kernel: initalized tarfs\r\n");
    printf("putchar@0x%lx\r\n", _putchar);
    
    // think its a good time to unmap identity mappings
    PmmAdjustBitmapPtr();
    KeRmvIdentityMap();
    printf("kernel: removed identity mapping from before.\r\n");
    
    int h = OsOpen("initrd:/hello.elf", 0);
    int sz = OsGetFileSize(h);
    printf("program is located at initrd:/hello.elf with %d size\r\n", sz);
    const char* buf = MmAllocate(sz);
    if (!buf) {
        printf("memory allocation fail.\r\n");
    } else {
        int st = OsRead(h, buf, sz);
        printf("bytes read: %d\r\n", st);
        OsClose(h);
        LdrElfExecute(buf);
    }
    while(1);
}

void KernelApplicationProc() {
    while(1) { __asm__("cli; hlt"); }
}
