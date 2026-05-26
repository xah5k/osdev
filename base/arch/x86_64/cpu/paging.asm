global _x86_64_load_pml4
_x86_64_load_pml4:
    ; mov rax, 0x000ffffffffff000
    ; and rdi, rax
    mov cr3, rdi
    ret

global _x86_64_get_pml4
_x86_64_get_pml4:
    mov rax, cr3
    ret