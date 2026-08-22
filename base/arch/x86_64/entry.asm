section .bss
; cuz uACPI crashes due to stack being too small
global kstack_bottom
global kstack_top
kstack_bottom:
    resb 65536
kstack_top:

section .text
extern bootboot
extern KernelBootstrapProc
extern KernelApplicationProc

global _start
_start:
    ; enable sse2 cuz every 64bit processor should have it anyway
    ; and to prevent mlibc init from crashing
    mov rax, cr0
    and rax, ~0x4 ; disable fpu emulation
    or rax, 0x2 ; mp
    mov cr0, rax

    mov rax, cr4
    or ax, 0x600
    mov cr4, rax

    mov rsp, kstack_top
    mov rbp, rsp
    sub rsp, 8
    jmp KernelBootstrapProc