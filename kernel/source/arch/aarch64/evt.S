.global __arch64_evt_load
.extern __arch64_int_handler

__arch64_evt_load:
    mov x0, sp
    mov x1, #1
    msr spsel, x1
    mov sp, x0

    adr x0, __arch64_exception_vector_table
    msr vbar_el1, x0

    ret

int_stub:
    // x30 and xzr are already saved.
    stp x28, x29, [sp, #-16]!
    stp x26, x27, [sp, #-16]!
    stp x24, x25, [sp, #-16]!
    stp x22, x23, [sp, #-16]!
    stp x20, x21, [sp, #-16]!
    stp x18, x19, [sp, #-16]!
    stp x16, x17, [sp, #-16]!
    stp x14, x15, [sp, #-16]!
    stp x12, x13, [sp, #-16]!
    stp x10, x11, [sp, #-16]!
    stp x8,  x9,  [sp, #-16]!
    stp x6,  x7,  [sp, #-16]!
    stp x4,  x5,  [sp, #-16]!
    stp x2,  x3,  [sp, #-16]!
    stp x0,  x1,  [sp, #-16]!

    mov x0, x30
    mov x1, sp
    mrs x2, esr_el1
    mrs x3, elr_el1
    mrs x4, spsr_el1
    mrs x5, far_el1
    bl aarch64_int_handler

    ldp x0,  x1,  [sp], #16
    ldp x2,  x3,  [sp], #16
    ldp x4,  x5,  [sp], #16
    ldp x6,  x7,  [sp], #16
    ldp x8,  x9,  [sp], #16
    ldp x10, x11, [sp], #16
    ldp x12, x13, [sp], #16
    ldp x14, x15, [sp], #16
    ldp x16, x17, [sp], #16
    ldp x18, x19, [sp], #16
    ldp x20, x21, [sp], #16
    ldp x22, x23, [sp], #16
    ldp x24, x25, [sp], #16
    ldp x26, x27, [sp], #16
    ldp x28, x29, [sp], #16
    ldp x30, xzr, [sp], #16

    eret

.align 11
__arch64_exception_vector_table:
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #0
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #1
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #2
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #3
    b int_stub

.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #4
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #5
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #6
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #7
    b int_stub

.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #8
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #9
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #10
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #11
    b int_stub

.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #12
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #13
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #14
    b int_stub
.align  7
    stp x30, xzr, [sp, #-16]!
    mov x30, #15
    b int_stub
