global main

main:
    mov rdi, epic
    mov rax, 0
    syscall
    jmp $

epic:
    db "hello userspace", 0
