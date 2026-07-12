[BITS 64]
[DEFAULT REL]

main:
    mov rbx, [rdi]
    mov rdi, 'H'
    call rbx
    mov rdi, 'I'
    call rbx
    ret