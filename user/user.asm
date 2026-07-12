[BITS 64]

main:
    mov rbx, 0xffffffffffe05374
    mov rdi, 'H'
    call rbx
    mov rdi, 'I'
    call rbx
    ret