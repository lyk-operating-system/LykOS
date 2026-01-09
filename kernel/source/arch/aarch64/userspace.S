.global __aarch64_thread_userspace_init

__aarch64_thread_userspace_init:
    /* Disable interrupts */
    msr DAIFSet, #0xf

    /* Load userspace entry + stack */
    ldr x0, [sp, #(31*8)]    /* userspace_init (optional arg) */
    ldr x1, [sp, #(32*8)]    /* entry PC */
    ldr x2, [sp, #(33*8)]    /* user_stack */

    /* Set EL0 stack */
    msr SP_EL0, x2

    mov x3, xzr
    mov x4, xzr
    mov x5, xzr
    mov x6, xzr
    mov x7, xzr
    mov x8, xzr
    mov x9, xzr
    mov x10, xzr
    mov x11, xzr
    mov x12, xzr
    mov x13, xzr
    mov x14, xzr
    mov x15, xzr
    mov x16, xzr
    mov x17, xzr
    mov x18, xzr
    mov x19, xzr
    mov x20, xzr
    mov x21, xzr
    mov x22, xzr
    mov x23, xzr
    mov x24, xzr
    mov x25, xzr
    mov x26, xzr
    mov x27, xzr
    mov x28, xzr
    mov x29, xzr
    mov x30, xzr

    /* Set userspace PC */
    msr ELR_EL1, x1

    eret
