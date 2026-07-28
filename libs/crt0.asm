section .text
extern main
global _start
_start:
    call main

    mov rdi, rax
    mov rax, 0
    int 0xFF
    ret ; no reach