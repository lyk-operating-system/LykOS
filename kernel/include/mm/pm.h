#pragma once

#include "arch/types.h"
#include "utils/list.h"
#include <stdatomic.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#define PM_MAX_PAGE_ORDER 10

typedef struct
{
    uintptr_t addr;
    uint8_t order;
    bool free;

    atomic_uint mapcount;
    atomic_uint refcount;

    list_node_t list_elem;
}
page_t;

static inline void pm_page_refcount_inc(page_t *page)
{
    atomic_fetch_add_explicit(&page->refcount, 1, memory_order_relaxed);
}

/**
 * @return true if the refcount reached zero, false otherwise.
 */
static inline bool pm_page_refcount_dec(page_t *page)
{
    return atomic_fetch_sub_explicit(&page->refcount, 1, memory_order_acq_rel) == 1;
}

/**
 * @brief Increment the number of mappings for this page.
 */
static inline void pm_page_map_inc(page_t *page)
{
    atomic_fetch_add_explicit(&page->mapcount, 1, memory_order_relaxed);
}

/**
 * @brief Decrement the number of mappings for this page.
 * @return true if the mapping count reached zero, false otherwise.
 */
static inline bool pm_page_map_dec(page_t *page)
{
    return atomic_fetch_sub_explicit(&page->mapcount, 1, memory_order_acq_rel) == 1;
}

uint8_t pm_pagecount_to_order(size_t pages);
size_t pm_order_to_pagecount(uint8_t order);

page_t *pm_phys_to_page(uintptr_t phys);

page_t *pm_alloc(uint8_t order);
void pm_free(page_t *page);

// Initialization

void pm_init();
