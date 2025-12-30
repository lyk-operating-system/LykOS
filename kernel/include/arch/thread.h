#pragma once

#include "mm/vm.h"
#include <stdint.h>

typedef struct arch_thread_context
{
#if defined(__x86_64__)
    struct arch_thread_context *self;
    uint64_t fs, gs;
    void *fpu_area;
#elif defined(__aarch64__)
#endif
    uint64_t rsp;
    uint64_t kernel_stack;
    uint64_t syscall_stack;
}
__attribute__((packed))
arch_thread_context_t;

void arch_thread_context_init(arch_thread_context_t *context, vm_addrspace_t *as, bool user, uintptr_t entry);
void arch_thread_context_switch(arch_thread_context_t *curr, arch_thread_context_t *next);
