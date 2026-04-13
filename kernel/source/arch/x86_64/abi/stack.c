#include "arch/x86_64/abi/stack.h"

#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "uapi/errno.h"
#include "utils/string.h"

int x86_64_abi_stack_setup(vm_addrspace_t *as, size_t stack_size,
                           const char *const argv[], const char *const envp[],
                           uint64_t *out_sp)
{
    uintptr_t stack_base = 0;
    int err = vm_map(
        as,
        0, stack_size,
        VM_PROTECTION_READ | VM_PROTECTION_WRITE,
        VM_MAP_ANON | VM_MAP_POPULATE | VM_MAP_PRIVATE,
        NULL, 0,
        &stack_base
    );
    log(LOG_WARN, "sb %p", stack_base);
    if (err != EOK)
        return err;

    uintptr_t sp = stack_base + stack_size;
    sp &= ~0xF;

    // Count args and envs
    size_t argc = 0;
    while (argv && argv[argc])
        argc++;
    size_t envc = 0;
    while (envp && envp[envc])
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
    for (int i = (int)envc - 1; i >= 0; i--)
    {
        sp -= sizeof(uintptr_t);
        uintptr_t val = (uintptr_t)envp_ptrs[i];
        vm_copy_to_user(as, (uintptr_t)sp, &val, sizeof(uintptr_t));
    }

    // Push 0
    sp -= sizeof(uint64_t);
    vm_zero_out_user(as, (uintptr_t)sp, sizeof(uint64_t));

    // Push argv pointers
    for (int i = (int)argc - 1; i >= 0; i--)
    {
        sp -= sizeof(uintptr_t);
        uintptr_t val = (uintptr_t)argv_ptrs[i];
        vm_copy_to_user(as, (uintptr_t)sp, &val, sizeof(uintptr_t));
    }

    // Push argc
    sp -= sizeof(uintptr_t);
    vm_copy_to_user(as, (uintptr_t)sp, &argc, sizeof(uintptr_t));

    *out_sp = sp;
    log(LOG_WARN, "sp %p", sp);
    return EOK;
}
