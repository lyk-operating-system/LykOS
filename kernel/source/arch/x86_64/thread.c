#include "arch/thread.h"
//
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

extern void __x86_64_thread_userspace_init();

extern __attribute__((naked)) void __thread_context_switch(arch_thread_context_t *new, arch_thread_context_t *old);

void arch_thread_context_init(arch_thread_context_t *context, vm_addrspace_t *as, bool user, uintptr_t entry)
{
    context->self = context;
    context->fs = context->gs = 0;

    if (user)
    {
        char *argv[] = { "test", NULL };
        char *envp[] = { NULL };

        context->kernel_stack = pm_alloc(0)->addr + HHDM + ARCH_PAGE_GRAN;
        context->rsp = (context->kernel_stack - sizeof(arch_thread_init_stack_kernel_t)) & (~0xF); // align as 16

        arch_thread_init_stack_user_t *init_stack = (arch_thread_init_stack_user_t *)context->rsp;
        *init_stack = (arch_thread_init_stack_user_t) {
            .userspace_init = __x86_64_thread_userspace_init,
            .entry = entry,
            .user_stack = x86_64_abi_stack_setup(as, ARCH_PAGE_GRAN * 8, argv, envp)
        };
    }
    else
    {
        context->kernel_stack = pm_alloc(0)->addr + HHDM + ARCH_PAGE_GRAN;
        context->rsp = context->kernel_stack - sizeof(arch_thread_init_stack_kernel_t);
        memset((void *)context->rsp, 0, sizeof(arch_thread_init_stack_kernel_t));
        ((arch_thread_init_stack_kernel_t *)context->rsp)->entry = entry;
    }

    uint8_t order = pm_pagecount_to_order(CEIL(x86_64_fpu_area_size, ARCH_PAGE_GRAN) / ARCH_PAGE_GRAN);
    context->fpu_area = (void *)(pm_alloc(order)->addr + HHDM);
    memset(context->fpu_area, 0, x86_64_fpu_area_size);
}

bool arch_thread_context_copy(arch_thread_context_t *dest, arch_thread_context_t *src)
{
    dest->self = dest;
    dest->fs = src->fs;
    dest->gs = src->gs;

    dest->kernel_stack = pm_alloc(0)->addr + HHDM + ARCH_PAGE_GRAN;
    dest->rsp = dest->kernel_stack - (src->kernel_stack - src->rsp);
    memcpy(
        (void *)(dest->kernel_stack - ARCH_PAGE_GRAN),
        (void *)(src->kernel_stack - ARCH_PAGE_GRAN),
        ARCH_PAGE_GRAN
    );

    if (src->fpu_area)
    {
        uint8_t order = pm_pagecount_to_order(CEIL(x86_64_fpu_area_size, ARCH_PAGE_GRAN) / ARCH_PAGE_GRAN);
        dest->fpu_area = (void *)(pm_alloc(order)->addr + HHDM);
        memcpy(dest->fpu_area, src->fpu_area, x86_64_fpu_area_size);
    }

    dest->syscall_stack = src->syscall_stack;

    return true;
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
