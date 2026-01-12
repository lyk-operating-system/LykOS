#include "mm/pm.h"

#include "arch/types.h"
#include "assert.h"
#include "bootreq.h"
#include "hhdm.h"
#include "log.h"
#include "mm/mm.h"
#include "panic.h"
#include "sync/spinlock.h"

static page_t *blocks;
static size_t block_count;
static list_t levels[PM_MAX_PAGE_ORDER + 1];
static spinlock_t slock = SPINLOCK_INIT;

uint8_t pm_pagecount_to_order(size_t pages)
{
    if (pages == 1)
        return 0;
    return (uint8_t)(64 - __builtin_clzll(pages - 1));
}

size_t pm_order_to_pagecount(uint8_t order)
{
    return (size_t)1 << order;
}

page_t *pm_phys_to_page(uintptr_t phys)
{
    return &blocks[phys / ARCH_PAGE_GRAN];
}

page_t *pm_alloc(uint8_t order)
{
    spinlock_acquire(&slock);

    int i = order;
    while (list_is_empty(&levels[i]))
    {
        i++;
        if (i > PM_MAX_PAGE_ORDER)
            return 0;
    }

    page_t *page = LIST_GET_CONTAINER(levels[i].head, page_t, list_elem);
    list_remove(&levels[i], levels[i].head);

    for (; i > order; i--)
    {
        // Right page.
        size_t r_idx = (page->addr / ARCH_PAGE_GRAN) ^ pm_order_to_pagecount(i - 1);
        page_t *right = &blocks[r_idx];
        right->order = i - 1;
        right->free = true;
        list_append(&levels[i - 1], &right->list_elem);
    }

    spinlock_release(&slock);

    page->order = order;
    page->free = false;
    page->mapcount = 0;
    page->refcount = 1;
    return page;
}

void pm_free(page_t *block)
{
    ASSERT(block->refcount == 1);

    spinlock_acquire(&slock);

    size_t idx = block->addr / ARCH_PAGE_GRAN;
    uint8_t i = block->order;

    while (i < PM_MAX_PAGE_ORDER)
    {
        size_t b_idx = idx ^ pm_order_to_pagecount(i);
        if (b_idx >= block_count)
            break;

        page_t *buddy = &blocks[b_idx];
        if (buddy->free == true && buddy->order == i)
        {
            list_remove(&levels[buddy->order], &buddy->list_elem);

            // The new merged block is on the left.
            block = idx < b_idx ? block : buddy;
            idx = idx < b_idx ? idx : b_idx;
            i++;
        }
        else
            break;
    }

    block->order = i;
    block->free = true;
    block->mapcount = 0;
    block->refcount = 0;
    list_append(&levels[i], &block->list_elem);

    spinlock_release(&slock);
}

// Initialization

void pm_init()
{
    if (bootreq_memmap.response == NULL
    || bootreq_memmap.response->entry_count == 0)
        panic("Invalid memory map provided by the bootloader!");

    for (int i = 0; i <= PM_MAX_PAGE_ORDER; i++)
        levels[i] = LIST_INIT;

    // Find the last usable memory entry to determine how many blocks our pmm
    // should manage.
    struct limine_memmap_entry *last_usable_entry;
    for (size_t i = 0; i < bootreq_memmap.response->entry_count; i++)
    {
        struct limine_memmap_entry *e = bootreq_memmap.response->entries[i];

        log(LOG_DEBUG, "[%2lu] type=%-2d base=%#018lx length=%#010lx (%4lu MiB + %4lu KiB)",
            i,
            e->type,
            e->base,
            e->length,
            e->length / MIB,
            e->length % MIB / KIB
        );

        if (e->type == LIMINE_MEMMAP_USABLE)
            last_usable_entry = e;
    }
    block_count = (last_usable_entry->base + last_usable_entry->length) / ARCH_PAGE_GRAN;

    // Find a usable memory entry at the start of which the blocks array will
    // be placed.
    for (size_t i = 0; i < bootreq_memmap.response->entry_count; i++)
    {
        struct limine_memmap_entry *e = bootreq_memmap.response->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE)
            if (e->length >= block_count * sizeof(page_t))
            {
                blocks = (page_t *)(e->base + HHDM);
                break;
            }
    }

    // Set each block's address and mark them as used for now.
    for (size_t i = 0; i < block_count; i++)
        blocks[i] = (page_t) {
            .addr = ARCH_PAGE_GRAN * i,
            .mapcount = 0,
            .order = 0,
            .free = false,
            .list_elem = LIST_NODE_INIT
        };

    // Iterate through each entry and set the blocks corresponding to a usable
    // memory entry as free using greedy.
    for (size_t i = 0; i < bootreq_memmap.response->entry_count; i++)
    {
        struct limine_memmap_entry *e = bootreq_memmap.response->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;

        uint8_t order = PM_MAX_PAGE_ORDER;
        uintptr_t addr = e->base;
        // We don't want to mark as free the pages that contain the pmm block array.
        // Remember the block array is placed at the start of a free region.
        if (addr == (uintptr_t)blocks - HHDM)
            addr += (block_count * sizeof(page_t) + (ARCH_PAGE_GRAN - 1)) / ARCH_PAGE_GRAN * ARCH_PAGE_GRAN;
        while (addr != e->base + e->length)
        {
            size_t span = pm_order_to_pagecount(order) * ARCH_PAGE_GRAN;

            if (addr + span > e->base + e->length || addr % span != 0)
            {
                order--;
                continue;
            }

            size_t idx = addr / ARCH_PAGE_GRAN;
            blocks[idx].order = order;
            blocks[idx].free = true;
            list_append(&levels[order], &blocks[idx].list_elem);

            addr += span;

            order = PM_MAX_PAGE_ORDER;
        }
    }

    log(LOG_INFO, "Phyiscal memory allocator initialized.");
}
