global exit
exit:
    mov rax, 0
    int 0xFF

global kill
kill:
    mov rax, 1
    int 0xFF
    ret