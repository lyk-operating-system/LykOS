global __thread_context_switch
extern sched_drop

%define THREAD_RSP_OFFSET 32

__thread_context_switch:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov qword [rdi + THREAD_RSP_OFFSET], rsp
    mov rsp, qword [rsi + THREAD_RSP_OFFSET]

    push rsi
    call sched_drop
    pop rsi

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    sti ; Unmask interrupts
    ret
