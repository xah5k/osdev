#include <sched/ipc/signal.h>
#include <sched/process.h>
#include <external/posix/signal.h>
#include <external/printf.h>
#include <ksyscall.h>
#include <arch/x86_64/archsyscall.h> // holy portability
#include <memory.h>
#include <sched/ipc/signaltrampoline.h>

KSTATUS KeSignalRegister(int Index, KeSignalHdlFunc handler, struct ProcessCtrlBlk* proc) {
    if (Index >= KE_SIGLIST_MAX) return KINVALID;
    if (Index < 0) return KINVALID;
    proc->Handlers[Index].Handler = handler;
    return KSUCCESS;
}

KSTATUS KeSignalDeregister(int Index, struct ProcessCtrlBlk* proc) {
    if (Index >= KE_SIGLIST_MAX) return KINVALID;
    if (Index < 0) return KINVALID;
    proc->Handlers[Index].Handler = (KeSignalHdlFunc)KE_SIGLIST_ADDR_DEFAULTKHDL;
    return KSUCCESS;
}

uint64_t SysSigReturn(uint64_t iframe, KE_SYSCALL_ARGS_UNUSED1) {
    CpuInterruptArgs* registers = (CpuInterruptArgs*)iframe;
    KeSignalUserFrame* SigFrame = (KeSignalUserFrame*)registers->rsp;
    registers->r15 = SigFrame->r15;
    registers->r14 = SigFrame->r14;
    registers->r13 = SigFrame->r13;
    registers->r12 = SigFrame->r12;
    registers->r11 = SigFrame->r11;
    registers->r10 = SigFrame->r10;
    registers->r9 = SigFrame->r9;
    registers->r8 = SigFrame->r8;
    registers->rdi = SigFrame->rdi;
    registers->rsi = SigFrame->rsi;
    registers->rdx = SigFrame->rdx;
    registers->rcx = SigFrame->rcx;
    registers->rbx = SigFrame->rbx;
    registers->rax = SigFrame->rax;
    registers->rbp = SigFrame->rbp;
    registers->rip = SigFrame->rip;
    registers->rflags = SigFrame->rflags;
    registers->rsp = SigFrame->rsp;
    ThrGetCurrent()->SigBlockedSet = SigFrame->SigBlockedSet;
    return 0;
}

uint64_t SysSigRegister(uint64_t SigIdx, uint64_t HandlerAddr, KE_SYSCALL_ARGS_UNUSED2) {
    KATTEMPT(KeSignalRegister(SigIdx, (KeSignalHdlFunc)HandlerAddr, ThrGetCurrent()->ParentProc) == KSUCCESS);
    return 0;
}

void KeSignalRegisterSyscalls() {
    KiRegisterSyscall(OS_SIGRETURN, SysSigReturn);
    KiRegisterSyscall(OS_SIGREGISTER, SysSigRegister);
}

KSTATUS KeSignalInitDef(struct ProcessCtrlBlk* proc) {
    // cuz KE_SIGLIST_ADDR_DEFAULT is zero
    // we dont have to do more work here cuz signal handling should 
    // check if proc->Handlers == KE_SIGLIST_ADDR_DEFAULT
    // to know if there isn't a handler registered by the program itself.
    memset((void*)proc->Handlers, 0, sizeof(KeSignalHdlObj) * KE_SIGLIST_MAX);
    return KSUCCESS;
}

// ugly switch statement
int KeSignalDefAct(int SigIdx) {
    switch(SigIdx) {
        case SIGSEGV:
        case SIGILL:
        case SIGINT:
        case SIGFPE:
        case SIGHUP:
        case SIGALRM:
        case SIGABRT:
        case SIGKILL: {
            return KE_SIGNAL_DEF_TERMINATE;
        }
        default: {
            return KE_SIGNAL_DEF_IGNORE;
        }
    }
}

void KeSignalHandle(ThreadCtrlBlk* Tcb, int SigIdx, KeSignalHdlObj* Signal, CpuInterruptArgs* OldCtx) {
    if (SigIdx >= KE_SIGLIST_MAX) {
        printf("ipc.signal: signal not handled (invalid SigIdx)\r\n");
        return;
    }
    if (SigIdx < 0) {
        printf("ipc.signal: signal not handled (invalid SigIdx)\r\n");
        return;
    }
    uint64_t NewUstackTop = (OldCtx->rsp - 128 - sizeof(KeSignalUserFrame)) & ~0xFULL;
    KeSignalUserFrame* Uframe = (KeSignalUserFrame*)NewUstackTop;
    Uframe->r15 = OldCtx->r15;
    Uframe->r14 = OldCtx->r14;
    Uframe->r13 = OldCtx->r13;
    Uframe->r12 = OldCtx->r12;
    Uframe->r11 = OldCtx->r11;
    Uframe->r10 = OldCtx->r10;
    Uframe->r9 = OldCtx->r9;
    Uframe->r8 = OldCtx->r8;
    Uframe->rdi = OldCtx->rdi;
    Uframe->rsi = OldCtx->rsi;
    Uframe->rdx = OldCtx->rdx;
    Uframe->rcx = OldCtx->rcx;
    Uframe->rbx = OldCtx->rbx;
    Uframe->rax = OldCtx->rax;
    Uframe->rbp = OldCtx->rbp;
    Uframe->rip = OldCtx->rip;
    Uframe->rflags = OldCtx->rflags;
    Uframe->rsp = OldCtx->rsp;
    Uframe->SigBlockedSet = Tcb->SigBlockedSet;
    Uframe->SigNum = SigIdx;
    uint64_t HandlerRsp = NewUstackTop - 8;
    uint64_t *RetAddr = (uint64_t*)HandlerRsp;
    *RetAddr = KE_LDR_SIGNAL_ADDR;
    OldCtx->rip = (uint64_t)Signal->Handler;
    OldCtx->rdi = SigIdx;
    OldCtx->rsp = HandlerRsp;
    Tcb->SigBlockedSet |= Signal->Mask | (1ULL << SigIdx);
}