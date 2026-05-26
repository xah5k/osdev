extern bootboot
extern KernelBootstrapProc
extern KernelApplicationProc
; extern kstack
global _start
_start:
    ; mov rsp, kstack
    mov eax, 1
    cpuid
    shr ebx, 24
    cmp [bootboot + 0xC], bx
    jne .ap
    jmp KernelBootstrapProc
.ap:
    jmp KernelApplicationProc