global _x86_64_get_stack 
_x86_64_get_stack:
    mov rax, rsp
    ret

global _x86_64_set_stack
_x86_64_set_stack:
    mov rsp, rdi
    ret

global _x86_64_load_gdt 
_x86_64_load_gdt:
    lgdt [rdi]

    ; reload data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    pop rdi
    push 0x08
    push rdi
    retfq

global _x86_64_load_idt
_x86_64_load_idt:
    lidt [rdi]
    ret

global _x86_64_pause
_x86_64_pause:
    pause

global _x86_64_ctxswitch
_x86_64_ctxswitch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    pushfq
    
    mov [rdi], rsp

    mov rsp, rsi

    popfq
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret