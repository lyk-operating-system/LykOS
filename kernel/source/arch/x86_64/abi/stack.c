#include "mm/mm.h"
#include "arch/x86_64/abi/stack.h"

uintptr_t x86_64_abi_stack_setup(vm_addrspace_t *as, size_t stack_size, char **argv, char **envp)
{
    uintptr_t stack_ptr;
    vm_map(
        as,
        0,
        stack_size,
        MM_PROT_FULL,
        VM_MAP_ANON | VM_MAP_POPULATE | VM_MAP_PRIVATE,
        NULL,
        0,
        &stack_ptr
    );
    uintptr_t stack = stack_ptr + stack_size - 1;
    stack &= ~0xF;
    return stack;
}
