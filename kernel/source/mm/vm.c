#include "mm/vm.h"

#include "arch/types.h"
#include "assert.h"
#include "bootreq.h"
#include "fs/vfs.h"
#include "hhdm.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "mm/pm.h"
#include "mm/vm/object.h"
#include "panic.h"
#include "sync/spinlock.h"
#include "uapi/errno.h"
#include "utils/list.h"
#include "utils/math.h"
#include <stdint.h>

/*
 * Global data
 */

vm_addrspace_t *vm_kernel_as;

/*
 * Segment utils
 */

static vm_segment_t *check_collision(vm_addrspace_t *as, uintptr_t base, size_t length)
{
    uintptr_t end = base + length - 1;

    FOREACH(n, as->segments)
    {
        vm_segment_t *seg = LIST_GET_CONTAINER(n, vm_segment_t, list_node);
        uintptr_t seg_base = seg->start;
        uintptr_t seg_end = seg->start + seg->length - 1;

        if (end >= seg_base && base <= seg_end)
            return seg;
    }

    return NULL;
}

static bool find_space(vm_addrspace_t *as, size_t length, uintptr_t *out)
{
    if (list_is_empty(&as->segments))
    {
        *out = as->limit_low;
        return true;
    }

    uintptr_t start = as->limit_low;
    FOREACH(n, as->segments)
    {
        vm_segment_t *seg = LIST_GET_CONTAINER(n, vm_segment_t, list_node);
        // If there's enough space between current start and this segment.
        if (start + length < seg->start)
            break;
        // Update start to point to the end of this segment.
        start = seg->start + seg->length;
    }

    // Check if there is space after the last segment.
    if (start + length - 1 <= as->limit_high)
    {
        *out = start;
        return true;
    }

    return false;
}

static void insert_seg(vm_addrspace_t *as, vm_segment_t *seg)
{
    list_node_t *pos = NULL;
    FOREACH(n, as->segments)
    {
        vm_segment_t *i = LIST_GET_CONTAINER(n, vm_segment_t, list_node);

        if (i->start < seg->start)
            pos = n;
        else
            break; // Given that the list is sorted, an earlier position must have been found.
    }

    if (pos)
        list_insert_after(&as->segments, pos, &seg->list_node);
    else
        list_prepend(&as->segments, &seg->list_node);
}

static vm_segment_t *find_seg(vm_addrspace_t *as, uintptr_t addr)
{
    FOREACH(n, as->segments)
    {
        vm_segment_t *seg = LIST_GET_CONTAINER(n, vm_segment_t, list_node);

        if (seg->start <= addr && addr - seg->start < seg->length)
            return seg;
    }

    return NULL;
}

// Page fault handler

bool vm_page_fault(vm_addrspace_t *as, uintptr_t virt, vm_protection_t type)
{
    vm_segment_t *seg = check_collision(as, virt, 1);
    if (!seg)
        return false;

    // Protection check
    if ((type.write && !(seg->prot.write)) ||
        (type.exec  && !(seg->prot.exec))  ||
        (type.read  && !(seg->prot.read)))
        return false;

    uintptr_t vaddr_aligned = FLOOR(virt, ARCH_PAGE_GRAN);
    size_t offset = ((vaddr_aligned - seg->start) / ARCH_PAGE_GRAN) + seg->offset;

    vm_object_t *obj = seg->object;
    page_t *page = NULL;
    if (!vm_object_get_page(obj, offset, &page))
        return false;

    /*
     * COW logic
     *
     * If:
     *  - write fault
     *  - mapping is private
     *  - page not already present in top-level object
     */
    if (type.write /*&& (seg->flags & VM_MAP_PRIVATE)*/)
    {
        page_t *existing = vm_object_lookup_page(obj, offset);

        if (!existing)
        {
            page_t *new_page = NULL;

            if (!obj->ops->copy_page(obj, offset, page, &new_page))
                return false;

            page = new_page;
        }
        else
            page = existing;
    }

    if (arch_paging_vaddr_to_paddr(as->page_map, vaddr_aligned, NULL))
        arch_paging_prot_page(
            as->page_map,
            vaddr_aligned,
            ARCH_PAGE_GRAN,
            seg->prot
        );
    else
        arch_paging_map_page(
            as->page_map,
            vaddr_aligned,
            page->addr,
            ARCH_PAGE_GRAN,
            seg->prot,
            VM_CACHE_STANDARD
        );

    return true;
}

// Mapping and unmapping

static int resolve_vaddr(vm_addrspace_t *as, uintptr_t vaddr, uintptr_t length, int flags, uintptr_t *out)
{
    if (vaddr < as->limit_low || length > as->limit_high - vaddr)
    {
        if (flags & (VM_MAP_FIXED | VM_MAP_FIXED_NOREPLACE))
            return EINVAL;
        if (!find_space(as, length, &vaddr))
            return ENOMEM;
    }

    if (check_collision(as, vaddr, length))
    {
        if (flags & VM_MAP_FIXED_NOREPLACE)
            return EEXIST;
        if (flags & VM_MAP_FIXED)
            return EINVAL;
        if (!find_space(as, length, &vaddr))
            return ENOMEM;
    }

    *out = vaddr;
    return EOK;
}

int vm_map(vm_addrspace_t *as, uintptr_t vaddr, size_t length,
           vm_protection_t prot, int flags,
           vm_object_t *obj, size_t offset,
           uintptr_t *out)
{
    spinlock_acquire(&as->slock);

    // Determine where the segment goes in the virtual address space.
    int ret = resolve_vaddr(as, vaddr, length, flags, &vaddr);
    if (ret != EOK)
    {
        spinlock_release(&as->slock);
        return ret;
    }

    // Manage assigned object
    if (!obj) // anon
    {
        obj = vm_object_create(VM_OBJ_ANON, length);
        if (!obj)
        {
            spinlock_release(&as->slock);
            return ENOMEM;
        }
    }
    else
        ref_get(&obj->refcount);

    // Initialize and insert the segment.
    vm_segment_t *seg = heap_alloc(sizeof(vm_segment_t));
    if (!seg)
    {
        spinlock_release(&as->slock);
        return ENOMEM;
    }
    *seg = (vm_segment_t) {
        .start = vaddr,
        .length = length,
        .prot = prot,
        .flags = flags,
        .object = obj,
        .offset = offset
    };
    insert_seg(as, seg);

    if (flags & VM_MAP_POPULATE)
    {
        for (size_t i = 0; i < length; i += ARCH_PAGE_GRAN)
        {
            uintptr_t curr_addr = vaddr + i;
            size_t curr_off = offset + i;

            page_t *page;
            if (!obj->ops->get_page(obj, curr_off, &page))
                panic("Fault handler failed!");

            arch_paging_map_page(
                as->page_map,
                curr_addr, page->addr,
                ARCH_PAGE_GRAN,
                prot, VM_CACHE_STANDARD
            );
        }
    }

    spinlock_release(&as->slock);
    *out = vaddr;
    return EOK;
}

int vm_unmap(vm_addrspace_t *as, uintptr_t vaddr, size_t length)
{
    spinlock_acquire(&as->slock);

    FOREACH(n, as->segments)
    {
        vm_segment_t *seg = LIST_GET_CONTAINER(n, vm_segment_t, list_node);

        if (seg->start == vaddr && seg->length == length)
        {
            for (size_t i = 0; i < seg->length; i += ARCH_PAGE_GRAN)
                arch_paging_unmap_page(as->page_map, seg->start + i);

            list_remove(&as->segments, n);
            heap_free(seg);

            spinlock_release(&as->slock);
            return EOK;
        }
    }

    spinlock_release(&as->slock);
    return ENOENT;
}

/*
 * Memory allocation
 */

typedef struct
{
    size_t obj_size;
}
vm_alloc_hdr_t;

void *vm_alloc(size_t size)
{
    size = CEIL(size + sizeof(vm_alloc_hdr_t), ARCH_PAGE_GRAN);

    spinlock_acquire(&vm_kernel_as->slock);

    uintptr_t out = 0;
    vm_map(
        vm_kernel_as,
        0, size,
        (vm_protection_t) {.write = true, .read = true}, VM_MAP_ANON | VM_MAP_POPULATE,
        NULL, 0,
        &out
    );

    spinlock_release(&vm_kernel_as->slock);

    return out ? (void *)out : NULL;
}

void vm_free(void *obj)
{
    spinlock_acquire(&vm_kernel_as->slock);

    vm_segment_t *seg = find_seg(vm_kernel_as, (uintptr_t)obj);
    vm_unmap(vm_kernel_as, (uintptr_t)obj, seg->length);

    spinlock_release(&vm_kernel_as->slock);
}

/*
 * Userspace utils
 */

size_t vm_copy_to_user(vm_addrspace_t *dest_as, uintptr_t dest, const void *src, size_t count)
{
    size_t i = 0;
    while (i < count)
    {
        size_t offset = (dest + i) % ARCH_PAGE_GRAN;
        uintptr_t phys;
        if(!arch_paging_vaddr_to_paddr(dest_as->page_map, dest + i, &phys))
        {
            // TODO: Handle this
            panic("Not mapped!");
        }

        size_t len = MIN(count - i, ARCH_PAGE_GRAN - offset);
        memcpy((void*)(phys + HHDM), src, len);
        i += len;
        src = (void *)((uintptr_t)src + len);
    }
    return i;
}

size_t vm_copy_from_user(vm_addrspace_t *src_as, void *dest, uintptr_t src, size_t count)
{
    size_t i = 0;
    while (i < count)
    {
        size_t offset = (src + i) % ARCH_PAGE_GRAN;
        uintptr_t phys;
        if (!arch_paging_vaddr_to_paddr(src_as->page_map, src + i, &phys))
        {
            // TODO: Handle this
            panic("Not mapped!");
        }

        size_t len = MIN(count - i, ARCH_PAGE_GRAN - offset);
        memcpy(dest, (void *)(phys + HHDM), len);
        i += len;
        dest = (void *)((uintptr_t)dest + len);
    }
    return i;
}

size_t vm_zero_out_user(vm_addrspace_t *dest_as, uintptr_t dest, size_t count)
{
    size_t i = 0;
    while (i < count)
    {
        size_t offset = (dest + i) % ARCH_PAGE_GRAN;
        uintptr_t phys;
        if(!arch_paging_vaddr_to_paddr(dest_as->page_map, dest + i, &phys))
        {
            // TODO: Handle this
            panic("Not mapped!");
        }

        size_t len = MIN(count - i, ARCH_PAGE_GRAN - offset);
        memset((void*)(phys + HHDM), 0, len);
        i += len;
    }
    return i;
}

// Map creation and destruction

vm_addrspace_t *vm_addrspace_create()
{
    vm_addrspace_t *map = heap_alloc(sizeof(vm_addrspace_t));
    *map = (vm_addrspace_t) {
        .segments = LIST_INIT,
        .page_map = arch_paging_map_create(),
        .limit_low = 0,
        .limit_high = HHDM,
        .slock = SPINLOCK_INIT
    };

    return map;
}

void vm_addrspace_destroy(vm_addrspace_t *as)
{
    vm_segment_t *seg = LIST_GET_CONTAINER(&as->segments, vm_segment_t, list_node);
    while (seg)
    {
        vm_segment_t *next = LIST_GET_CONTAINER(seg->list_node.next, vm_segment_t, list_node);
        vm_unmap(as, seg->start, seg->length);
        heap_free(seg);
        seg = next;
    }

    arch_paging_map_destroy(as->page_map);
    heap_free(as);
}

// Address space cloning

vm_addrspace_t *vm_addrspace_clone(vm_addrspace_t *parent_as)
{
    spinlock_acquire(&parent_as->slock);

    // Create new address space
    vm_addrspace_t *new_as = vm_addrspace_create();
    if (!new_as)
    {
        spinlock_release(&parent_as->slock);
        return NULL;
    }
    new_as->limit_low = parent_as->limit_low;
    new_as->limit_high = parent_as->limit_high;

    FOREACH(node, parent_as->segments)
    {
        vm_segment_t *parent_seg = LIST_GET_CONTAINER(node, vm_segment_t, list_node);
        vm_object_t *shared_backing = parent_seg->object;

        // Create shadows for child and parent
        vm_object_t *child_shadow = vm_object_create(VM_OBJ_SHADOW, shared_backing->size);
        if (!child_shadow)
            goto fail;
        vm_object_t *parent_shadow = vm_object_create(VM_OBJ_SHADOW, shared_backing->size);
        if (!parent_shadow)
        {
            vm_object_unref(child_shadow);
            goto fail;
        }
        child_shadow->source.shadow.parent = shared_backing; // Make them point to original object
        parent_shadow->source.shadow.parent = shared_backing;
        vm_object_ref(shared_backing); // Update ref count
        vm_object_ref(shared_backing);

        parent_seg->object = parent_shadow;

        // Create segment
        vm_segment_t *child_seg = heap_alloc(sizeof(vm_segment_t));
        if (!child_seg)
        {
            // TODO: cleanup
            goto fail;
        }
        memcpy(child_seg, parent_seg, sizeof(vm_segment_t));
        child_seg->object = child_shadow;

        list_append(&new_as->segments, &child_seg->list_node);

        for (size_t i = 0; i < parent_seg->length; i += ARCH_PAGE_GRAN)
        {
            uintptr_t curr_addr = parent_seg->start + i;
            arch_paging_prot_page(
                parent_as->page_map,
                curr_addr, ARCH_PAGE_GRAN,
                (vm_protection_t) {.write = false, .read = true, .exec = true}
            );
        }
    }

    spinlock_release(&parent_as->slock);
    return new_as;

fail:
    spinlock_release(&parent_as->slock);
    vm_addrspace_destroy(new_as);
    return NULL;
}

// Address space loading

void vm_addrspace_load(vm_addrspace_t *as)
{
    arch_paging_map_load(as->page_map);
}

// Initialization

static void do_big_mappings(uintptr_t vaddr, uintptr_t paddr, size_t length)
{
    vm_segment_t *seg = heap_alloc(sizeof(vm_segment_t));
    *seg = (vm_segment_t) {
        .start = vaddr,
        .length = length,
        .offset = paddr
    };
    insert_seg(vm_kernel_as, seg);

    size_t i = 0; // Progress
    while (i < length)
    {
        size_t remaining = length - i;
        size_t page_size;

        // Try largest -> smallest
        for (int j = ARCH_PAGE_SIZES_LEN - 1; j >= 0; j--)
        {
            page_size = ARCH_PAGE_SIZES[j];
            if ((vaddr + i) % page_size == 0
            &&  (paddr + i) % page_size == 0
            &&  remaining >= page_size)
                break;
        }

        arch_paging_map_page(
            vm_kernel_as->page_map,
            vaddr + i,
            paddr + i,
            page_size,
            VM_PROTECTION_FULL,
            VM_CACHE_STANDARD
        );

        i += page_size;
    }
}

void vm_init()
{
    arch_paging_init();

    vm_kernel_as = vm_addrspace_create();
    vm_kernel_as->limit_low = HHDM;
    vm_kernel_as->limit_high = ARCH_KERNEL_MAX_VIRT;

    // Directly map the first 4GiB of system memory to the HHDM
    // region as per the Limine specification.
    do_big_mappings(
        HHDM,
        0,
        4 * GIB
    );

    // Map the kernel physical region to its virtual base.
    do_big_mappings(
        bootreq_kernel_addr.response->virtual_base,
        bootreq_kernel_addr.response->physical_base,
        2 * GIB
    );

    // Map usable physical memory regions.
    for (size_t i = 0; i < bootreq_memmap.response->entry_count; i++)
    {
        struct limine_memmap_entry *e = bootreq_memmap.response->entries[i];
        if (e->type == LIMINE_MEMMAP_RESERVED
        ||  e->type == LIMINE_MEMMAP_BAD_MEMORY)
            continue;

        uintptr_t start = FLOOR(e->base, ARCH_PAGE_GRAN);
        uintptr_t end = CEIL(e->base + e->length, ARCH_PAGE_GRAN);
        uint64_t length = end - start;

        log(LOG_DEBUG,
            "[%2lu] type=%-2d phys=%#018lx virt=%#018lx len=%#010lx (%4llu MiB + %4llu KiB)",
            i,
            e->type,
            start,
            start + HHDM,
            length,
            length / MIB,
            (length % MIB) / KIB
        );

        if (end < 4 * GIB)
            continue;

        if (start < 4 * GIB)
            start = 4 * GIB;
        length = end - start;
        if (length == 0)
            continue;

        do_big_mappings(
            start + HHDM,
            start,
            length
        );
    }

    vm_addrspace_load(vm_kernel_as);

    log(LOG_INFO, "Virtual memory initialized.");
}
