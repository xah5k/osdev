#pragma once
#ifdef __x86_64__
#include <arch/x86_64/syscallidx.h>
// loader/process creation function should map these bytes at sm address for sigreturn
/* btw the signal js does this:
    mov rax, (OS_SIGRETURN)
    mov rdi, rsp
    int 0xFF
    ret
*/
#define KE_LDR_SIGNAL_ADDR 0x100000

static const unsigned char __signal_trampoline[]  = {
    0xb8, OS_SIGRETURN, 0x00, 0x00, 0x00, 0xcd, 0xff, 0xc3
};
#endif