#pragma once
#include <stdint.h>
#include <external/bootboot.h>
#ifdef __x86_64__
#include <arch/x86_64/cpu/paging.h>
#include <arch/x86_64/cpu/cpu.h>
#include <arch/x86_64/cpu/gdt.h>
#include <arch/x86_64/acpi.h>
#endif

#define ARCHS_X86_64 "x86-64"
struct NetArpEntry;
typedef struct {
    uint64_t ptr;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint32_t scanline;
} Framebuffer;
typedef struct {
    uint8_t Mac[6];
    uint8_t Ip[4];
    struct NetArpEntry* ArpHead;
} KeNetInfo;

typedef struct KernelInformation {
    BOOTBOOT* bootinfo;
    AcpiRsdtTable* rsdt;
    void* initrd; // pointer to initrd
    Framebuffer* fb; // pointer to fb
    #ifdef __x86_64__
    CpuTss* tss; // pointer to tss
    #else
    uint64_t Rsv0;
    #endif
    struct ProcessCtrlBlk* ProcessListHead; // list of processes
    #ifdef __x86_64__
    CpuFeatures* cpufeats;
    #else
    uint64_t Rsv1;
    #endif
    struct ProcessCtrlBlk* CurrentProcess;
    struct ProcessCtrlBlk* KernelProcess;
    int DriverExt2Load;
    int FwType; // 0 = bios, 1 = uefi
    KeNetInfo net;
} KernelInformation;

typedef enum {
    UNREGISTERED_INTERRUPT,
    INTENTIONAL_INVOCATION,
    KERNEL_CORE_COMP_FAIL,
    KERNEL_ACPI_FIRMWARE_FATAL
} BugcheckCode;
struct ProcessCtrlBlk* KernelGetCurrentProc();
KernelInformation* KernelGetInformation();
void KernelUnlockRsLck();

#include <kstatus.h> // im not gonna deal with stupid header shi

#define KFORWARD(x, c, i) x[c+i]

#define KDBG printf("dbg %s:%d:%s\r\n", __FILE__, __LINE__, __FUNCTION__);
#define KDBGC(x) x("dbg %s:%d:%s\r\n", __FILE__, __LINE__, __FUNCTION__);
#define KATTEMPT(x) if (!(x))  { printf("x=%lu\r\n", x); KdBugcheck2(KERNEL_CORE_COMP_FAIL, NULL, __LINE__, __FILE__); }
#define KSUCCESS(x) if (x != KSUCCESS) { printf("x=%lu\r\n", x); KdBugcheck2(KERNEL_CORE_COMP_FAIL, NULL, __LINE__, __FILE__); }
void KdBugcheck(BugcheckCode code, CpuInterruptArgs* registers);
void KdBugcheck2(BugcheckCode code, CpuInterruptArgs* registers, int line, char* filename);