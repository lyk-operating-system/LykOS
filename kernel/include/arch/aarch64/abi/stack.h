#include "mm/vm.h"
#include <stdint.h>

uintptr_t aarch64_abi_stack_setup(vm_addrspace_t *as, size_t stack_size, char **argv, char **envp);
