#pragma once

#include "arch/paging.h"
#include "fs/vfs.h"
#include "mm/pm.h"
#include "sync/spinlock.h"
#include "utils/list.h"
#include "utils/xarray.h"
#include <stddef.h>
#include <stdint.h>

#define VM_MAP_PRIVATE         0x01
#define VM_MAP_SHARED          0x02
#define VM_MAP_ANON            0x04
#define VM_MAP_FIXED           0x08
#define VM_MAP_FIXED_NOREPLACE 0x10
#define VM_MAP_POPULATE        0x20

typedef struct vm_pager_ops vm_pager_ops_t;
typedef struct vm_object vm_object_t;

struct vm_pager_ops
{
    /*
     * Called when a page is missing from the XArray.
     * The pager should allocate a physical page, fill it with data,
     * and return it.
     */
    int (*get_page)(vm_object_t *obj, uintptr_t offset, page_t **out_page);

    // Called when the VMM wants to push a dirty page to disk.
    int (*put_page)(vm_object_t *obj, page_t *page);

    // Called when the vm_object is being destroyed.
    void (*cleanup)(vm_object_t *obj);
};

struct vm_object
{
    size_t size;
    xarray_t pages;

    const vm_pager_ops_t *pager;
    void *pager_data; // eg. vnode

    vm_object_t *shadow;

    atomic_uint refcount;
    spinlock_t slock;
};

typedef struct
{
    uintptr_t start;
    size_t length;

    int prot;
    int flags;

    // Object backing this segment.
    vm_object_t *object;
    // Offset into the object where this segment starts.
    uintptr_t offset;

    list_node_t list_node;
}
vm_segment_t;

typedef struct
{
    list_t segments;
    arch_paging_map_t *page_map;
    uintptr_t limit_low;
    uintptr_t limit_high;

    spinlock_t slock;
}
vm_addrspace_t;

// Global data

extern vm_addrspace_t *vm_kernel_as;

// Mapping and unmapping

int vm_map(vm_addrspace_t *as, uintptr_t vaddr, size_t length,
           int prot, int flags,
           vnode_t *vn, uintptr_t offset,
           uintptr_t *out);
int vm_unmap(vm_addrspace_t *as, uintptr_t vaddr, size_t length);

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
