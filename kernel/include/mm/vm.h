#pragma once

#include "arch/paging.h"
#include "sync/spinlock.h"
#include "utils/list.h"
#include <stddef.h>
#include <stdint.h>

// Forward declarrations

typedef struct vm_object vm_object_t;
typedef struct vm_segment vm_segment_t;
typedef struct vm_addrspace vm_addrspace_t;

//

#define VM_FAULT_WRITE 0

#define VM_MAP_PRIVATE         0x01
#define VM_MAP_SHARED          0x02
#define VM_MAP_ANON            0x04
#define VM_MAP_FIXED           0x08
#define VM_MAP_FIXED_NOREPLACE 0x10
#define VM_MAP_POPULATE        0x20

struct vm_segment
{
    uintptr_t start;
    size_t length;

    vm_protection_t prot;
    int flags;

    vm_object_t *object;
    size_t offset;

    list_node_t list_node;
};

struct vm_addrspace
{
    list_t segments;
    arch_paging_map_t *page_map;
    uintptr_t limit_low;
    uintptr_t limit_high;

    spinlock_t slock;
};

// Global data

extern vm_addrspace_t *vm_kernel_as;

// Page fault handler

bool vm_page_fault(vm_addrspace_t *as, uintptr_t virt, vm_pf_type pf_type);

// Mapping and unmapping

int vm_map(vm_addrspace_t *as, uintptr_t vaddr, size_t length,
           vm_protection_t prot, int flags,
           vm_object_t *obj, size_t offset,
           uintptr_t *out);
int vm_unmap(vm_addrspace_t *as, uintptr_t vaddr, size_t length);

// Memory allocation

void *vm_alloc(size_t size);
void vm_free(void *obj);

// Userspace utils

size_t vm_copy_to_user(vm_addrspace_t *dest_as, uintptr_t dest, const void *src, size_t count);
size_t vm_copy_from_user(vm_addrspace_t *src_as, void *dest, uintptr_t src, size_t count);
size_t vm_zero_out_user(vm_addrspace_t *dest_as, uintptr_t dest, size_t count);

// Address space creation and destruction

vm_addrspace_t *vm_addrspace_create();
void vm_addrspace_destroy(vm_addrspace_t *as);

// Address space cloning

vm_addrspace_t *vm_addrspace_clone(vm_addrspace_t *as);

// Address space loading

void vm_addrspace_load(vm_addrspace_t *as);

// Initialization

void vm_init();
