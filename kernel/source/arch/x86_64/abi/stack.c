#include "arch/x86_64/abi/stack.h"

#include "mm/heap.h"
#include "mm/mm.h"
#include "utils/string.h"

uintptr_t x86_64_abi_stack_setup(vm_addrspace_t *as, size_t stack_size, char **argv, char **envp)
{
    uintptr_t stack_base;
    vm_map(
        as,
        0x1000, stack_size,
        VM_PROTECTION_READ | VM_PROTECTION_WRITE,
        VM_MAP_ANON | VM_MAP_POPULATE | VM_MAP_PRIVATE | VM_MAP_FIXED,
        NULL, 0,
        &stack_base
    );

    uintptr_t sp = stack_base + stack_size;
    sp &= ~0xF;

    // Count args and envs
    size_t argc = 0;
    while (argv[argc])
        argc++;
    size_t envc = 0;
    while (envp[envc])
        envc++;

    // Push strings and save their addr
    CLEANUP char **argv_ptrs = heap_alloc(argc * sizeof(char *));
    CLEANUP char **envp_ptrs = heap_alloc(envc * sizeof(char *));
    for (size_t i = 0; i < argc; i++)
    {
        size_t len = strlen(argv[i]) + 1;
        sp -= len;
        vm_copy_to_user(as, (uintptr_t)sp, argv[i], len);
        argv_ptrs[i] = (char *)sp;
    }
    for (size_t i = 0; i < envc; i++)
    {
        size_t len = strlen(envp[i]) + 1;
        sp -= len;
        vm_copy_to_user(as, (uintptr_t)sp, envp[i], len);
        envp_ptrs[i] = (char *)sp;
    }
    sp &= ~0xF;

    // Push 0
    sp -= sizeof(uint64_t);
    vm_zero_out_user(as, (uintptr_t)sp, sizeof(uint64_t));

    // Push envp pointers
    for (int i = envc - 1; i >= 0; i--)
    {
        sp -= sizeof(uintptr_t);
        uintptr_t val = (uintptr_t)envp_ptrs[i];
        vm_copy_to_user(as, (uintptr_t)sp, &val, sizeof(uintptr_t));
    }

    // Push 0
    sp -= sizeof(uint64_t);
    vm_zero_out_user(as, (uintptr_t)sp, sizeof(uint64_t));

    // Push argv pointers
    for (int i = argc - 1; i >= 0; i--)
    {
        sp -= sizeof(uintptr_t);
        uintptr_t val = (uintptr_t)argv_ptrs[i];
        vm_copy_to_user(as, (uintptr_t)sp, &val, sizeof(uintptr_t));
    }

    // Push argc
    sp -= sizeof(uintptr_t);
    vm_copy_to_user(as, (uintptr_t)sp, &argc, sizeof(uintptr_t));

    return sp;
}
