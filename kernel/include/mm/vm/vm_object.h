#pragma once

#include "fs/vfs.h"
#include "mm/pm.h"
#include "sync/spinlock.h"
#include "utils/ref.h"
#include "utils/xarray.h"

// Forward declarrations

typedef struct page page_t;
typedef struct vnode vnode_t;

typedef enum vm_object_type vm_object_type_t;
typedef struct vm_object_ops vm_object_ops_t;
typedef struct vm_object vm_object_t;

//

enum vm_object_type
{
    VM_OBJ_ANON,
    VM_OBJ_VNODE,
    VM_OBJ_PHYS,
    VM_OBJ_SHADOW,
};

struct vm_object_ops
{
    bool (*get_page)(vm_object_t *obj, size_t offset, page_t **page_out);
    bool (*put_page)(vm_object_t *obj, page_t *page);
    bool (*copy_page)(vm_object_t *obj, size_t offset, page_t *src, page_t **dst_out);

    void (*destroy)(vm_object_t *obj);
};

struct vm_object
{
    vm_object_type_t type;
    size_t size;
    unsigned flags;

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

// Lifecycle

vm_object_t *vm_object_create(vm_object_type_t type, size_t size);

void vm_object_ref(vm_object_t *obj);
void vm_object_unref(vm_object_t *obj);

// Page Management

bool vm_object_get_page(vm_object_t *obj, size_t offset, page_t **page_out);
void vm_object_insert_page(vm_object_t *obj, page_t *page, size_t offset);
void vm_object_remove_page(vm_object_t *obj, size_t offset);
page_t *vm_object_lookup_page(vm_object_t *obj, size_t offset);

// Sync

int vm_object_sync(vm_object_t *obj, size_t start, size_t end);
