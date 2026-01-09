#include "arch/thread.h"
//
#include "arch/aarch64/abi/stack.h"
#include "arch/lcpu.h"
#include "arch/types.h"
#include "hhdm.h"
#include "mm/mm.h"
#include "mm/pm.h"

typedef struct
{
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
    void (*userspace_init)();
    uintptr_t entry;
    uint64_t user_stack;
}
__attribute__((packed))
arch_thread_init_stack_user_t;

typedef struct
{
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
    uintptr_t entry;
}
__attribute__((packed))
arch_thread_init_stack_kernel_t;

extern void __aarch64_thread_userspace_init();

extern __attribute__((naked)) void __thread_context_switch(arch_thread_context_t *new, arch_thread_context_t *old);

void arch_thread_context_init(arch_thread_context_t *context, vm_addrspace_t *as, bool user, uintptr_t entry)
{
    if (user)
    {
        char *argv[] = { "test", NULL };
        char *envp[] = { NULL };

        context->kernel_stack = pm_alloc(0)->addr + HHDM + ARCH_PAGE_GRAN;
        context->rsp = (context->kernel_stack - sizeof(arch_thread_init_stack_kernel_t)) & (~0xF); // align as 16

        arch_thread_init_stack_user_t *init_stack = (arch_thread_init_stack_user_t *)context->rsp;
        *init_stack = (arch_thread_init_stack_user_t) {
            .userspace_init = __aarch64_thread_userspace_init,
            .entry = entry,
            .user_stack = aarch64_abi_stack_setup(as, ARCH_PAGE_GRAN * 8, argv, envp)
        };
    }
    else
    {
        context->kernel_stack = pm_alloc(0)->addr + HHDM + ARCH_PAGE_GRAN;
        context->rsp = context->kernel_stack - sizeof(arch_thread_init_stack_kernel_t);
        memset((void *)context->rsp, 0, sizeof(arch_thread_init_stack_kernel_t));
        ((arch_thread_init_stack_kernel_t *)context->rsp)->entry = entry;
    }
}

void arch_thread_context_switch(arch_thread_context_t *curr, arch_thread_context_t *next)
{
    arch_lcpu_thread_reg_write((size_t)next);
    __thread_context_switch(curr, next); // This function calls `sched_drop` for `curr` too.
}
