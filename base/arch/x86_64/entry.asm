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
.loop:
    cli
    hlt
    jmp .loop
.ap:
    jmp KernelApplicationProc