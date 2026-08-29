#include <kernel.h>
#include <sched/process.h>
#include <hal/hal.h>
#include <ksyscall.h>
#include <kedriver.h>
#include <printfwrapper.h>

const char* BugcheckTable[4] = {
    "UNREGISTERED_INTERRUPT",
    "INTENTIONAL_INVOCATION",
    "KERNEL_CORE_COMP_FAIL",
    "KERNEL_ACPI_FIRMWARE_FATAL"
};

static void KdTraceStack(uint32_t Frames) {
    HalStackFr* Frame;
    HAL_GET_BP(Frame);
    printf("kdbg: stack trace: \r\n");
    for (uint32_t i = 0; i < Frames; i++) {
        if (Frame) {
            printf("    0x%lx    \r\n", Frame->ip);
            Frame = Frame->bp;
        }
    }
}

void KdBugcheck(BugcheckCode code, CpuInterruptArgs* registers) {
    if (ThrGetCurrent()->privilege == SCHED_PRIV_USER) {
        printf("kdbg: bugcheck in usermode.\r\n");
        if (HAL_GET_INUM(registers) == 14) {
            printf("kdbg: page fault.\r\n");
            uint64_t faultaddr;
            HAL_GET_CR2(faultaddr);
            printf("fault address = 0x%lx ip = 0x%lx err = 0x%lx\r\n", faultaddr, HAL_GET_IP(registers), HAL_GET_ERR(registers));
            while (1) {
                HAL_HALT();
            }
        } else {
            printf("kdbg: fault (intvec=%d ip=0x%lx)", HAL_GET_INUM(registers), HAL_GET_IP(registers));
            printf("kdbg: killing user task..\r\n");
            KE_SYSCALL_CALL_ARG1(SysKill, ThrGetCurrent()->ParentProc->pid);
            return;
        }
    }
    printf("kdbg: unrecoverable bugcheck\r\n");
    printf("kdbg: bugcheck type: %s [0x%x]\r\n", BugcheckTable[code], code);
    if (registers) {
        if (HAL_GET_INUM(registers) == 14) {
            printf("kdbg: page fault.\r\n");
            uint64_t faultaddr;
            HAL_GET_CR2(faultaddr);
            printf("fault address = 0x%lx ip = 0x%lx err = 0x%lx\r\n", faultaddr, HAL_GET_IP(registers), HAL_GET_ERR(registers));
        }
        HalDumpRegisters(registers);
    } else {
        printf("kdbg: no interrupt frame provided (non interrupt?)\r\n");
    }
    KdTraceStack(5);
    if (ThrGetCurrent()->privilege == SCHED_PRIV_KERNEL) {
        printf("kdbg: halting\r\n");
        HAL_HALT();
    }
}
KE_EXPORT_SYMBOL(KdBugcheck);

void KdBugcheck2(BugcheckCode code, CpuInterruptArgs* registers, int line, char* filename) {
    printf("kernel: unrecoverable bugcheck\r\n");
    printf("kernel: bugcheck type: %s [0x%x]\r\n", BugcheckTable[code], code);
    if (registers) {
        HalDumpRegisters(registers);
    } else {
        printf("kernel: no interrupt frame provided (non interrupt?)\r\n");
    }
    if (line && filename) {
        printf("kernel: file and line: %s:%d\r\n", filename, line);
    }
    KdTraceStack(5);
    printf("kernel: halting\r\n");
    while (1) {
        HAL_HALT();
    }
}
KE_EXPORT_SYMBOL(KdBugcheck2);
