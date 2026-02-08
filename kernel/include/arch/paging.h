#pragma once

#include "mm/mm.h"
#include <stddef.h>
#include <stdint.h>

// Forward declaration

typedef struct arch_paging_map arch_paging_map_t;

// Mapping and unmapping

int arch_paging_map_page(arch_paging_map_t *map, uintptr_t vaddr, uintptr_t paddr, size_t size, vm_protection_t prot, vm_cache_t cache);

int arch_paging_unmap_page(arch_paging_map_t *map, uintptr_t vaddr);

// Utils

bool arch_paging_vaddr_to_paddr(const arch_paging_map_t *map, uintptr_t vaddr, uintptr_t *out_paddr);

// Map creation and destruction

arch_paging_map_t *arch_paging_map_create();
void arch_paging_map_destroy(arch_paging_map_t *map);

// Map loading

void arch_paging_map_load(arch_paging_map_t *map);

// Init

void arch_paging_init();
