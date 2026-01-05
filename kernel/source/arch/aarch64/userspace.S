.global __aarch64_thread_userspace_init

__aarch64_thread_userspace_init:
    // x0 = pointer to arch_thread_init_stack_user_t

    // Load user SP
    ldr x1, [x0, #(sizeof(arch_thread_init_stack_user_t) - 8)]
    msr SP_EL0, x1

    // Load ELR_EL1 (user PC)
    ldr x1, [x0, #(sizeof(arch_thread_init_stack_user_t) - 16)]
    msr ELR_EL1, x1

    // Set up SPSR_EL1:
    // EL0t, interrupts enabled
    mov x1, #(0b0000)          // M[3:0] = EL0t
    orr x1, x1, #(1 << 7)      // I = 0 (IRQs enabled)
    orr x1, x1, #(1 << 6)      // F = 0
    orr x1, x1, #(1 << 9)      // D = 0
    msr SPSR_EL1, x1

    // Restore GPRs x0–x30
    ldp x2,  x3,  [x0], #16
    ldp x4,  x5,  [x0], #16
    ldp x6,  x7,  [x0], #16
    ldp x8,  x9,  [x0], #16
    ldp x10, x11, [x0], #16
    ldp x12, x13, [x0], #16
    ldp x14, x15, [x0], #16
    ldp x16, x17, [x0], #16
    ldp x18, x19, [x0], #16
    ldp x20, x21, [x0], #16
    ldp x22, x23, [x0], #16
    ldp x24, x25, [x0], #16
    ldp x26, x27, [x0], #16
    ldp x28, x29, [x0], #16
    ldr x30, [x0]

    eret
