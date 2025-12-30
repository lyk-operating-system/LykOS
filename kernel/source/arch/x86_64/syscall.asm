global x86_64_arch_syscall_entry

%define KERNEL_STACK_OFFSET 40
%define SYSCALL_STACK_OFFSET 48

extern syscall_table
extern syscall_table_length

section .text
x86_64_arch_syscall_entry:
    swapgs
    mov qword [gs:SYSCALL_STACK_OFFSET], rsp
    mov rsp, qword [gs:KERNEL_STACK_OFFSET]

    ; RAX and RDX are not preserved because they hold the syscall return value and errno, respectively.

    push rbx
    push rcx
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

    ; Validate interrupt number.
    cmp rax, qword [syscall_table_length]
    jge .invalid_syscall

    mov rax, [syscall_table + rax * 8]
    cmp rax, 0
    je .invalid_syscall
    ;

    ; As per the Sys V ABI RDI, RSI, RDX, RCX, R8, and R9 are function arguments.
    ; RDI, RSI, RDX contain the first 3 arguments; this also matches the first 3 arguments for the Sys V ABI.
    ; The SYSCALL instruction sets RCX, thus it cannot be used. R10 is used to replace it.
    ; Lastly, R8 and R9 are passed which will match Sys V again.
    mov rcx, r10
    call rax

    .invalid_syscall:

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
    pop rcx
    pop rbx

    mov rsp, qword [gs:SYSCALL_STACK_OFFSET]
    swapgs
    o64 sysret
