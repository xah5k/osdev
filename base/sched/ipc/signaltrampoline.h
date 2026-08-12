#pragma once

// loader/process creation function should map these bytes at sm address for sigreturn
/* btw the signal js does this:
    mov rax, 34 (OS_SIGRETURN)
    mov rdi, rsp
    int 0xFF
*/
#define KE_LDR_SIGNAL_ADDR 0x100000

static const unsigned char __signal_trampoline[]  = {
    0xb8, 0x22, 0x00, 0x00, 0x00, 0xcd, 0xff
};