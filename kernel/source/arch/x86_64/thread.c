#include "arch/thread.h"

#include "arch/lcpu.h"
#include "arch/types.h"
#include "arch/x86_64/abi/stack.h"
#include "arch/x86_64/fpu.h"
#include "arch/x86_64/msr.h"
#include "hhdm.h"
#include "mm/mm.h"
#include "mm/pm.h"
#include "utils/math.h"

typedef struct
{
#if defined(__x86_64__)
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
#elif defined(__aarch64__)
    uint64_t x30;
    uint64_t x29;
    uint64_t x28;
    uint64_t x27;
    uint64_t x26;
    uint64_t x25;
    uint64_t x24;
    uint64_t x23;
    uint64_t x22;
    uint64_t x21;
    uint64_t x20;
    uint64_t x19;
    uint64_t x18;
    uint64_t x17;
    uint64_t x16;
    uint64_t x15;
    uint64_t x14;
    uint64_t x13;
    uint64_t x12;
    uint64_t x11;
    uint64_t x10;
    uint64_t x9;
    uint64_t x8;
    uint64_t x7;
    uint64_t x6;
    uint64_t x5;
    uint64_t x4;
    uint64_t x3;
    uint64_t x2;
    uint64_t x1;
    uint64_t x0;
#endif
    void (*userspace_init)();
    uintptr_t entry;
    uint64_t user_stack;
}
__attribute__((packed))
arch_thread_init_stack_user_t;

typedef struct
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uintptr_t entry;
}
__attribute__((packed))
arch_thread_init_stack_kernel_t;

extern void x86_64_thread_userspace_init();

extern __attribute__((naked)) void __thread_context_switch(arch_thread_context_t *new, arch_thread_context_t *old);

void arch_thread_context_init(arch_thread_context_t *context, vm_addrspace_t *as, bool user, uintptr_t entry)
{
    context->self = context;
    context->fs = context->gs = 0;

    if (user)
    {
        char *argv[] = { "test", NULL };
        char *envp[] = { NULL };

        context->kernel_stack = pm_alloc(0) + HHDM + ARCH_PAGE_GRAN;
        context->rsp = (context->kernel_stack - sizeof(arch_thread_init_stack_kernel_t)) & (~0xF); // align as 16

        arch_thread_init_stack_user_t *init_stack = (arch_thread_init_stack_user_t *)context->rsp;
        *init_stack = (arch_thread_init_stack_user_t) {
            .userspace_init = x86_64_thread_userspace_init,
            .entry = entry,
            .user_stack = x86_64_abi_stack_setup(as, ARCH_PAGE_GRAN * 8, argv, envp)
        };
    }
    else
    {
        context->kernel_stack = pm_alloc(0) + HHDM + ARCH_PAGE_GRAN;
        context->rsp = context->kernel_stack - sizeof(arch_thread_init_stack_kernel_t);
        memset((void *)context->rsp, 0, sizeof(arch_thread_init_stack_kernel_t));
        ((arch_thread_init_stack_kernel_t *)context->rsp)->entry = entry;
    }

    uint8_t order = pm_pagecount_to_order(CEIL(x86_64_fpu_area_size, ARCH_PAGE_GRAN) / ARCH_PAGE_GRAN);
    context->fpu_area = (void*)(pm_alloc(order) + HHDM);
    memset(context->fpu_area, 0, x86_64_fpu_area_size);
}

void arch_thread_context_switch(arch_thread_context_t *curr, arch_thread_context_t *next)
{
    // FS & GS
    curr->fs = x86_64_msr_read(X86_64_MSR_FS_BASE);
    curr->gs = x86_64_msr_read(X86_64_MSR_KERNEL_GS_BASE);
    x86_64_msr_write(X86_64_MSR_FS_BASE, next->fs);
    x86_64_msr_write(X86_64_MSR_KERNEL_GS_BASE, next->gs);
    // FPU
    x86_64_fpu_save(curr->fpu_area);
    x86_64_fpu_restore(next->fpu_area);

    arch_lcpu_thread_reg_write((size_t)next);

    __thread_context_switch(curr, next); // This function calls `sched_drop` for `curr` too.
}
