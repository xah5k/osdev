#include <stdint.h>
#include <memory.h>
#include <external/limine.h>
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
#include <hal/init.h>
#include <hal/dbgout.h>
#include <fs/tar.h>
#include <fs/krnlfs.h>
#include <sched/sched.h>
#include <sched/ipc/msg.h>
#include <kedriver.h>
#include <util/util.h>
#include <ksyscall.h>
#include <fb.h>
#include <util/shell.h>
#include <disk/ahci.h>
#include <disk/ptable.h>
#include <net/net.h>
#include <mouse.h>

// limine request stuff
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
// framebuffer
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// memory map
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

// acpi
__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

// initrd
__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;


void KernelBootstrapProc();
void KernelApplicationProc();

static KernelInformation* gkInfo;
Spinlock KernelResourceLock = {ATOMIC_FLAG_INIT};

void _putchar(char character) {
    HalPutChar(character);
    FbPutc(character);
}


void KernelUnlockRsLck() {
}

struct ProcessCtrlBlk* KernelGetCurrentProc() {
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
    kinfo->DriverExt2Load = 1;
    return;
}

static KernelInformation* KeCreateKinfo() {
    KernelInformation* kInfo = (KernelInformation*)P2V(PmmAllocate());
    KATTEMPT(kInfo);
    kInfo->initrd = NULL;
    kInfo->fb = (Framebuffer*)P2V(PmmAllocate());
    KATTEMPT(kInfo->fb);
    KATTEMPT(framebuffer_request.response);
    KATTEMPT(framebuffer_request.response->framebuffer_count > 0);
    kInfo->fb->ptr = (uint64_t)framebuffer_request.response->framebuffers[0]->address;
    kInfo->fb->size = framebuffer_request.response->framebuffers[0]->height * framebuffer_request.response->framebuffers[0]->pitch;
    kInfo->fb->width = framebuffer_request.response->framebuffers[0]->width;
    kInfo->fb->height = framebuffer_request.response->framebuffers[0]->height;
    kInfo->fb->scanline = framebuffer_request.response->framebuffers[0]->pitch;
    kInfo->fb->bpp = framebuffer_request.response->framebuffers[0]->bpp;
    kInfo->FwType = 1;
    memset((void*)&kInfo->net, 0, sizeof(KeNetInfo));
    kInfo->net.Ip[0] = 192;
    kInfo->net.Ip[1] = 168;
    kInfo->net.Ip[2] = 100;
    kInfo->net.Ip[3] = 1;
    kInfo->net.DhcpXid = 0x3903F326;
    kInfo->net.ArpHead = NULL;
    KeParseConfig(kInfo);
    return (KernelInformation*)(kInfo);
}

extern uint64_t PmmLargestFreeMemorySize;



static void KeInitalizeDrivers() {
    int count = 0;
    const char** list = KeDrvBuildDriverList(&count);
    for (int i = 0; i < count; i++) {
        if (list[i]) {
            int handle = OsOpen(list[i], 0);
            if (handle < 0)  { 
                printf("kernel: failed to acquire handle for driver file. (returned %d)\r\n", handle);
                printf("attempted to do OsOpen(\"%s\", 0)", list[i]);
                continue;
            } else {
                int sz = OsGetFileSize(handle);
                void* buf = MmAllocate(sz);
                if (!buf)  { printf("kernel: failed to allocate buffer.\r\n"); continue; }
                int read __attribute__((unused)) = OsRead(handle, buf, sz);
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
                printf("kernel: driver return KSTATUS %d\r\n", init);
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
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == 0) {
        HAL_HALT();
    }
    // setup pmm
    KSUCCESS(PmmInitalize(memmap_request.response, hhdm_request.response->offset));
    gMmuVOffset = hhdm_request.response->offset;
    // create kernel information structure
    gkInfo = KeCreateKinfo();
    KATTEMPT(gkInfo);
    KSUCCESS(HalSetupMmu(gkInfo));
    MmuMapPage((pagetable*)P2V(HalGetPageTable()), (virtaddr)((uint64_t)gkInfo->fb + MMU_PHYS_OFFSET), (physaddr)gkInfo->fb, MMU_PAGE_BIT_P_PRESENT | MMU_PAGE_BIT_RW_WRITABLE);
    KATTEMPT(rsdp_request.response);
    gkInfo->rsdp = (AcpiRsdpTable*)rsdp_request.response->address;
    gkInfo->rsdt = (AcpiRsdtTable*)(P2V(gkInfo->rsdp->Rsdt));
    // setup higher memory management
    KeMmInitalize();
    // setup sched structures
    SchedInitalize(gkInfo);
    #ifdef __x86_64__
	HalInitalize(gkInfo);    
    #endif
    KeInitalizeDiskParts();
    KeRegisterSyscalls();
    // gkInfo->initrd = (void*)(bootboot.initrd_ptr + MMU_PHYS_OFFSET);
    KATTEMPT(module_request.response);
    KATTEMPT(module_request.response->module_count > 0);
    struct limine_file *initrd_file = module_request.response->modules[0];
    void *InitrdPtr = initrd_file->address;
    gkInfo->initrd = InitrdPtr;
    TarInitalizeVfs(gkInfo->initrd);
    KeFbAsConsole();
    KSUCCESS(HalRmvIdentityMap());
    KeInitalizeDrivers();
    KrnlFsInitalize();
    printf("kernel: krnlfs: initalized.\r\n");
    // network related
    uint8_t b[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    NetArpTableAdd(&gkInfo->net.ArpHead, b, b);
    if (!NetGetLinkedList()) {
        printf("kernel: skipping dhcp configuration, no NIC found.\r\n");
    } else {
        KSUCCESS(NetDhcpConfigure(NetGetLinkedList()));
    }
    KeUtilShell();
    HAL_HALT_WITHINT();
}

void KernelApplicationProc() {
    while(1) {HAL_HALT(); }
}
