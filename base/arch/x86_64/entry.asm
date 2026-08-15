section .bss
; cuz uACPI crashes due to stack being too small
kstack_bottom:
    resb 65536
kstack_top:

section .text
extern bootboot
extern KernelBootstrapProc
extern KernelApplicationProc

global _start
_start:
    ; mov rsp, kstack
    mov eax, 1
    cpuid
    shr ebx, 24
    cmp [bootboot + 0xC], bx
    jne .ap
    mov rsp, kstack_top
    mov rbp, rsp
    sub rsp, 8
    jmp KernelBootstrapProc
.loop:
    cli
    hlt
    jmp .loop
.ap:
    jmp KernelApplicationProc