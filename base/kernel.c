#include <stdint.h>
#include <memory.h>
#include <external/bootboot.h>
#include <printfwrapper.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <memory.h>
#include "fs/vfs.h"
#include "sched/process.h"
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
#include <kedriver.h>
#include <util/util.h>
#include <ksyscall.h>
#include <fb.h>
#include <util/shell.h>
#include <disk/ahci.h>
#include <disk/ptable.h>
#include <net/net.h>
void KernelBootstrapProc();
void KernelApplicationProc();

static KernelInformation* gkInfo;
Spinlock KernelResourceLock = {ATOMIC_FLAG_INIT};

extern BOOTBOOT bootboot;               // see bootboot.h
extern unsigned char environment[4096]; // configuration, UTF-8 text key=value pairs
extern uint8_t fb;                      // linear framebuffer mapped

void _putchar(char character) {
    HalPutChar(character);
    FbPutc(character);
}

const char* BugcheckTable[4] = {
    "UNREGISTERED_INTERRUPT",
    "INTENTIONAL_INVOCATION",
    "KERNEL_CORE_COMP_FAIL",
    "KERNEL_ACPI_FIRMWARE_FATAL"
};

void KdBugcheck(BugcheckCode code, CpuInterruptArgs* registers) {
    if (ThrGetCurrent()->privilege == SCHED_PRIV_USER) {
        printf("kernel: bugcheck in usermode.\r\n");
        if (registers->intnum == 14) {
            printf("kernel: page fault.\r\n");
            uint64_t faultaddr;
            asm volatile("mov %%cr2, %0" : "=r"(faultaddr));
            printf("fault address = 0x%lx rip = 0x%lx err = 0x%lx\r\n", faultaddr, registers->rip, registers->errcode);
            while (1) {
                __asm__("cli; hlt");
            }
        } else {
            printf("kernel: fault (intvec=%d)", registers->intnum);
            printf("kernel: killing user task..\r\n");
            KE_SYSCALL_CALL_ARG1(SysKill, ThrGetCurrent()->ParentProc->pid);
            return;
        }
    }
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
    if (ThrGetCurrent()->privilege == SCHED_PRIV_KERNEL) {
        printf("kernel: halting\r\n");
        while (1) {
            asm ("cli; hlt");
        }
    }
}
KE_EXPORT_SYMBOL(KdBugcheck);

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
KE_EXPORT_SYMBOL(KdBugcheck2);


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
KE_EXPORT_SYMBOL(KernelGetInformation);


KSTATUS KeMmInitalize() {
    KSUCCESS(VmmInitalize());
    KSUCCESS(MmHeapInitalize());
    return KSUCCESS;
}


static void KeParseConfig(KernelInformation* kinfo) {
    // messy code
    for (int i = 0; i < 4096; i++) {
        if (KFORWARD(environment, i, 0) == 'd' && KFORWARD(environment, i, 1) == 'r' && KFORWARD(environment, i, 2) == 'v') {
            if (KFORWARD(environment, i, 4) == 'e' && KFORWARD(environment, i, 5) == 'x' && KFORWARD(environment, i, 6) == 't') {
                int val = KFORWARD(environment, i, 9) - '0';
                kinfo->DriverExt2Load = val;
            }
        }
    }
    printf("\r\n");
}
static KernelInformation* KeCreateKinfo() {
    KernelInformation* kInfo = PmmAllocate();
    kInfo->bootinfo = &bootboot;
    kInfo->initrd = (void*)(bootboot.initrd_ptr);
    kInfo->fb = (Framebuffer*)PmmAllocate();
    KATTEMPT(kInfo->fb);
    kInfo->fb->ptr = bootboot.fb_ptr;
    kInfo->fb->size = bootboot.fb_size;
    kInfo->fb->width = bootboot.fb_width;
    kInfo->fb->height = bootboot.fb_height;
    kInfo->fb->scanline = bootboot.fb_scanline;
    if (bootboot.arch.x86_64.efi_ptr) {
        kInfo->FwType = 1;
    } else {
        kInfo->FwType = 0;
    }
    memset((void*)&kInfo->net, 0, sizeof(KeNetInfo));
    kInfo->net.Ip[0] = 192;
    kInfo->net.Ip[1] = 168;
    kInfo->net.Ip[2] = 100;
    kInfo->net.Ip[3] = 1;
    kInfo->net.ArpHead = NULL;
    KeParseConfig(kInfo);
    return kInfo;
}

extern uint64_t PmmLargestFreeMemorySize;



static void KeInitalizeDrivers() {
    int count = 0;
    const char** list = KeDrvBuildDriverList(&count);
    for (int i = 0; i < count; i++) {
        if (list[i]) {
            int handle = OsOpen(list[i], 0);
            if (handle < 0)  { 
                continue;
            } else {
                int sz = OsGetFileSize(handle);
                void* buf = MmAllocate(sz);
                if (!buf)  { printf("kernel: failed to allocate buffer.\r\n"); continue; }
                int read = OsRead(handle, buf, sz);
                KeDriverObj* driver = NULL;
                KSTATUS result = LdrElfDriverExec(buf, &driver);
                MmFree(buf);
                OsClose(handle);
                if (result != KSUCCESS) {
                    printf("kernel: failed to load driver.\r\n");
                    continue;
                }
                if (driver == NULL) {
                    printf("kernel: failed to get driver object.\r\n");
                    continue;
                }
                if (!driver->Initalize) {
                    printf("kernel: error: driver load fail. no initalize func???\r\n");
                }
                KSTATUS init = driver->Initalize(driver);
                if (init != KSUCCESS) {
                    printf("kernel: warn: driver load fail. discarding.\r\n");
                    MmFree(driver);
                    continue;
                }
                KeDrvRegisterDriver(driver);
            }
        }
    }
}

void KeFbAsConsole() {
    gkInfo->fb->ptr = P2V(gkInfo->fb->ptr);
    int h = OsOpen("initrd:/boot/font.psf", 0);
    int sz = OsGetFileSize(h);
    const char* buf = MmAllocate(sz);
    OsRead(h, (void*)buf, sz);
    FbTextInitalize((void*)buf, gkInfo->fb);
    OsClose(h);
}

void KeInitalizeDiskParts() {
    uint8_t* buffer = PmmAllocate();
    uint8_t* vbuf = (uint8_t*)P2V(buffer);
    memset(vbuf, 0, 4096);
    KSTATUS r = AhciPortRead(AhciGetPort(0), 1, 1, buffer);
    if (r == KFAIL) {
        printf("kernel: failed to write to port.\r\n");
        PmmFree(buffer);
        return;
    } else if (r == KHUNG) {
        printf("kernel: port is hung or being used.\r\n");
        PmmFree(buffer);
        return;
    }
    KSTATUS r2 = PtableEnumerate((void*)vbuf);
    if (r2 != KSUCCESS) {
        printf("kernel: failed to call PtableEnumerate.\r\n");
        PmmFree(buffer);
        return;
    }
}

void KernelBootstrapProc() {
    // setup pmm
    PmmInitalize(&bootboot);
    // create kernel information structure
    gkInfo = KeCreateKinfo();
    KATTEMPT(gkInfo);
    // setup paging
    KSUCCESS(HalSetupMmu(gkInfo));
    MmuMapPage(HalGetPageTable(), (virtaddr)((uint64_t)gkInfo->fb + MMU_PHYS_OFFSET), (physaddr)gkInfo->fb, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    gkInfo = (KernelInformation*)((uint64_t)gkInfo + MMU_PHYS_OFFSET);
    gkInfo->fb = (Framebuffer*)((uint64_t)gkInfo->fb + MMU_PHYS_OFFSET);
    gkInfo->rsdt = (void*)((uint64_t)bootboot.arch.x86_64.acpi_ptr + gMmuVOffset);
    // setup higher memory management
    KeMmInitalize();
    // setup sched structures
    SchedInitalize(gkInfo);
    #ifdef __x86_64__
	HalInitalize(gkInfo);    
    #endif
    KeInitalizeDiskParts();
    KeRegisterSyscalls();
    gkInfo->initrd = (void*)(bootboot.initrd_ptr + MMU_PHYS_OFFSET);
    TarInitalizeVfs(gkInfo->initrd);
    KeFbAsConsole();
    PmmAdjustBitmapPtr();
    KSUCCESS(HalRmvIdentityMap());
    // also add the mapping for 255.255.255.255
    uint8_t b[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    NetArpTableAdd(&gkInfo->net.ArpHead, b, b);
    KeInitalizeDrivers();
    printf("kernel: doing dhcp discovery.\r\n");
    KSTATUS r = NetDhcpDiscover(NetGetLinkedList());
    printf("kernel: KSTATUS 0x%lx\r\n", r);
    KeUtilShell();
    while(1) { __asm__("hlt"); }
}

void KernelApplicationProc() {
    while(1) { __asm__("cli; hlt"); }
}
