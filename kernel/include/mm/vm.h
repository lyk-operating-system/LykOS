#pragma once

#include "arch/paging.h"
#include "sync/spinlock.h"
#include "utils/list.h"
#include "utils/ref.h"
#include "utils/xarray.h"
#include <stddef.h>
#include <stdint.h>

// Forward declarrations

typedef struct page page_t;
typedef struct vnode vnode_t;

typedef enum vm_object_type vm_object_type_t;
typedef struct vm_object_ops vm_object_ops_t;
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

enum vm_object_type
{
    VM_OBJ_ANON,
    VM_OBJ_VNODE,
    VM_OBJ_PHYS,
    VM_OBJ_SHADOW
};

struct vm_object_ops
{
    page_t *(*fault)(vm_object_t *obj, size_t offset, uintptr_t addr, unsigned type);
    void (*destroy)(vm_object_t *obj);
    vm_object_t* (*copy)(vm_object_t *obj);
};

struct vm_object
{
    vm_object_type_t type;
    size_t size;
    xarray_t cached_pages;
    vm_object_ops_t *ops;

    union
    {
        struct
        {
            vnode_t *vnode;
            uintptr_t offset;
        }
        vnode;

        struct
        {
            uintptr_t paddr;
        }
        phys;

        struct
        {
            struct vm_object *parent;
        }
        shadow;
    }
    source;

    list_node_t list_node;
    spinlock_t slock;
    ref_t refcount;
};

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
