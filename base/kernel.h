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

typedef struct {
    uint64_t ptr;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint32_t scanline;
} Framebuffer;
typedef struct KernelInformation {
    BOOTBOOT* bootinfo;
    AcpiRsdtTable* rsdt;
    void* initrd; // pointer to initrd
    Framebuffer* fb; // pointer to fb
    CpuTss* tss; // pointer to tss
    struct ProcessCtrlBlk* ProcessListHead; // list of processes
    CpuFeatures* cpufeats;
    struct ProcessCtrlBlk* CurrentProcess;
    struct ProcessCtrlBlk* KernelProcess;
    int DriverExt2Load;
} KernelInformation;

typedef enum {
    UNREGISTERED_INTERRUPT,
    INTENTIONAL_INVOCATION,
    KERNEL_CORE_COMP_FAIL,
} BugcheckCode;
struct ProcessCtrlBlk* KernelGetCurrentProc();
KernelInformation* KernelGetInformation();
void KernelUnlockRsLck();

typedef enum {
    KSUCCESS,
    KFAIL,
    KUNSUPPORTED,
    KINVALID,
    KOOMERR,
    KHUNG,
    KRESEND,
    KDOUBLEFREE
} KSTATUS;

#define KFORWARD(x, c, i) x[c+i]

#define KDBG printf("dbg %s:%d:%s\r\n", __FILE__, __LINE__, __FUNCTION__);
#define KATTEMPT(x) if (!(x)) KdBugcheck2(KERNEL_CORE_COMP_FAIL, NULL, __LINE__, __FILE__)

void KdBugcheck(BugcheckCode code, CpuInterruptArgs* registers);
void KdBugcheck2(BugcheckCode code, CpuInterruptArgs* registers, int line, char* filename);