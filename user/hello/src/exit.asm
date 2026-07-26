global exit
exit:
    mov rax, 0
    int 0xFF

global kill
kill:
    mov rax, 1
    int 0xFF

global spawn
spawn:
    mov rax, 2
    int 0xFF
    ret

global conwrite
conwrite:
    mov rax, 3
    int 0xFF
    ret

global yield
yield:
    mov rax, 4
    int 0xFF
    ret

global sbrk
sbrk:
    mov rax, 7
    int 0xFF
    ret