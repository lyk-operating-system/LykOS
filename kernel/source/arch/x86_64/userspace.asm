global x86_64_thread_userspace_init

section .data
    x87cw  dw  (3<<8)|(1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|(1<<0)
    mxcsr  dd  (1<<12)|(1<<11)|(1<<10)|(1<<9)|(1<<8)|(1<<7)

x86_64_thread_userspace_init:
    pop rcx ; sysret return address, basically program entry point

    cli
    swapgs

    pop rax ; userspace stack pointer
    mov rsp, rax

    xor rbp, rbp
    xor rax, rax
    xor rbx, rbx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    fldcw   [rel x87cw] ; load x87 control word
    ldmxcsr [rel mxcsr] ; load SSE MXCSR

    mov r11, (1 << 9) | (1 << 1) ; Sets interrupt flag and reserved bit.
    o64 sysret
