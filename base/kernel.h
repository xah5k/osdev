#pragma once
#include <stdint.h>
#include <external/bootboot.h>
#ifdef __x86_64__
#include <arch/x86_64/cpu/paging.h>
#include <arch/x86_64/cpu/cpu.h>
#include <arch/x86_64/acpi.h>
#endif


typedef struct {
    uint64_t ptr;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint32_t scanline;
} Framebuffer;
typedef struct {
    BOOTBOOT* bootinfo;
    AcpiRsdtTable* rsdt;
    void* initrd; // pointer to initrd
    Framebuffer* fb; // pointer to fb
    struct ProcessCtrlBlk* ProcessListHead; // list of processes
    struct ProcessCtrlBlk* CurrentProcess;
} KernelInformation;

typedef enum {
    UNREGISTERED_INTERRUPT,
    INTENTIONAL_INVOCATION
} BugcheckCode;
struct ProcessCtrlBlk* KernelGetCurrentProc();

void KernelBugcheck(BugcheckCode code, CpuInterruptArgs* registers);