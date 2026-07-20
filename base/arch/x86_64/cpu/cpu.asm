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
    mov ax, 0x28
    ltr ax
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

global _x86_64_usjmp 
_x86_64_usjmp:
    cli
    mov rax, 0x1B
    push rax
    
    push rsi
    mov rax, 0x0202
    push rax
    mov rax, 0x23
    push rax
    push rdi
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8,  r8
    xor r9,  r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    iretq

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