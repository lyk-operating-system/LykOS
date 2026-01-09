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
    atomic_uint mapcount;
    uint8_t order;
    bool free;

    list_node_t list_elem;
}
page_t;

uint8_t pm_pagecount_to_order(size_t pages);
size_t pm_order_to_pagecount(uint8_t order);

page_t *pm_phys_to_page(uintptr_t phys);

page_t *pm_alloc(uint8_t order);
void pm_free(page_t *page);

// Initialization

void pm_init();
