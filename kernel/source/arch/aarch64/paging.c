#include "arch/paging.h"

#include "hhdm.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "mm/pm.h"

#define PTE_VALID       (1ull <<  0)
#define PTE_TABLE       (1ull <<  1)
#define PTE_BLOCK       (0ull <<  1)
#define PTE_PAGE_4K     (1ull <<  1)
#define PTE_READONLY    (1ull <<  6)
#define PTE_USER        (1ull <<  7)
#define PTE_ACCESS      (1ull << 10)
#define PTE_XN          (1ull << 54)

#define PTE_ADDR_MASK(VALUE) ((VALUE) & 0x000FFFFFFFFFF000ull)

typedef uint64_t pte_t;

struct arch_paging_map
{
    pte_t *pml4[2];
};

static uint64_t translate_prot(int prot)
{
    uint64_t pte_prot = 0;

    if (!(prot & MM_PROT_WRITE))
        pte_prot |= PTE_READONLY;
    if (prot & MM_PROT_USER)
        pte_prot |= PTE_USER;
    if (!(prot & MM_PROT_EXEC))
        pte_prot |= PTE_XN;

    return pte_prot;
}

// Mapping and unmapping

int arch_paging_map_page(arch_paging_map_t *map, uintptr_t vaddr, uintptr_t paddr, size_t size, int prot)
{
    pte_t _prot = translate_prot(prot);
    pte_t *table = map->pml4[vaddr >= HHDM ? 1 : 0]; // Is higher half?

    size_t indices[] = {
        (vaddr >> 39) & 0x1FF, // Level 0
        (vaddr >> 30) & 0x1FF, // Level 1
        (vaddr >> 21) & 0x1FF, // Level 2
        (vaddr >> 12) & 0x1FF  // Level 3
    };

    size_t target_level = (size == 1 * GIB) ? 1 : (size == 2 * MIB) ? 2 : 3;
    for (size_t level = 0; level < target_level; level++)
    {
        size_t idx = indices[level];

        if (!(table[idx] & PTE_VALID))
        {
            uintptr_t phys = pm_alloc(0)->addr;
            pte_t *next_table = (pte_t *)(phys + HHDM);
            memset(next_table, 0, 0x1000);
            table[idx] = phys | PTE_VALID | PTE_TABLE | PTE_ACCESS;
        }

        pm_page_refcount_inc(pm_phys_to_page(PTE_ADDR_MASK((uintptr_t)table - HHDM)));
        table = (pte_t *)(PTE_ADDR_MASK(table[idx]) + HHDM);
    }

    size_t leaf_idx = indices[target_level];
    pm_page_refcount_inc(pm_phys_to_page(PTE_ADDR_MASK((uintptr_t)table - HHDM)));
    uint64_t type_bit = (target_level == 3) ? PTE_TABLE : 0 /* block */;
    table[leaf_idx] = paddr | PTE_VALID | type_bit | PTE_ACCESS | _prot;

    return 0;
}

int arch_paging_unmap_page(arch_paging_map_t *map, uintptr_t vaddr)
{
    size_t indices[] = {
        (vaddr >> 39) & 0x1FF, // Level 0
        (vaddr >> 30) & 0x1FF, // Level 1
        (vaddr >> 21) & 0x1FF, // Level 2
        (vaddr >> 12) & 0x1FF  // Level 3
    };

    pte_t *tables[4];
    tables[0] = map->pml4[vaddr >= HHDM ? 1 : 0]; // Is higher half?

    // Descend
    size_t level;
    for (level = 0; level <= 2; level++)
    {
        size_t idx = indices[level];
        pte_t entry = tables[level][idx];

        if (!(entry & PTE_VALID)) // Not mapped, nothing to do.
            return -1;
        if (!(entry & PTE_TABLE)) // Huge Page, end walk early.
            break;

        tables[level + 1] = (pte_t *)(PTE_ADDR_MASK(entry) + HHDM);
    }

    // Clear the mapping.
    size_t leaf_idx = indices[level];
    tables[level][leaf_idx] = 0;

    // Ascend
    for (int l = (int)level; l >= 0; l--)
    {
        uintptr_t table_phys = (uintptr_t)tables[l] - HHDM;

        // Check if the table is empty.
        if (!pm_page_refcount_dec(pm_phys_to_page(table_phys)))
            break;

        // Don't free the root table.
        if (l > 0)
        {
            size_t parent_idx = indices[l - 1];
            tables[l - 1][parent_idx] = 0;

            pm_free(pm_phys_to_page(table_phys));
        }
    }

    // Flush TLB
    // vae1is = virt addr + EL1 + inner shareable
    uintptr_t vpage = vaddr >> 12;
    asm volatile("tlbi vae1is, %0" :: "r"(vpage) : "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");

    return 0;
}

// Utils

bool arch_paging_vaddr_to_paddr(arch_paging_map_t *map, uintptr_t vaddr, uintptr_t *out_paddr)
{
    uint64_t l0e = (vaddr >> 39) & 0x1FF;
    uint64_t l1e = (vaddr >> 30) & 0x1FF;
    uint64_t l2e = (vaddr >> 21) & 0x1FF;
    uint64_t l3e = (vaddr >> 12) & 0x1FF;

    pte_t *l0 = map->pml4[vaddr >= HHDM ? 1 : 0];
    pte_t l0ent = l0[l0e];
    if (!(l0ent & PTE_VALID))
        return false;

    pte_t *l1 = (pte_t *)(PTE_ADDR_MASK(l0ent) + HHDM);
    pte_t l1ent = l1[l1e];
    if (!(l1ent & PTE_VALID))
        return false;

    // 1 GiB block
    if (!(l1ent & PTE_TABLE))
    {
        *out_paddr = PTE_ADDR_MASK(l1ent) + (vaddr & ((1ull << 30) - 1));
        return true;
    }

    pte_t *l2 = (pte_t *)(PTE_ADDR_MASK(l1ent) + HHDM);
    pte_t l2ent = l2[l2e];
    if (!(l2ent & PTE_VALID))
        return false;

    // 2 MiB block
    if (!(l2ent & PTE_TABLE))
    {
        *out_paddr = PTE_ADDR_MASK(l2ent) + (vaddr & ((1ull << 21) - 1));
        return true;
    }

    pte_t *l3 = (pte_t *)(PTE_ADDR_MASK(l2ent) + HHDM);
    pte_t l3ent = l3[l3e];
    if (!(l3ent & PTE_VALID))
        return false;

    // 4 KiB page
    *out_paddr = PTE_ADDR_MASK(l3ent) + (vaddr & 0xFFF);
    return true;
}

// Map creation and destruction

pte_t *higher_half_pml4;
bool ttbr1_loaded = false;

arch_paging_map_t *arch_paging_map_create()
{
    arch_paging_map_t *map = heap_alloc(sizeof(arch_paging_map_t));
    map->pml4[0] = (pte_t *)(pm_alloc(0)->addr + HHDM);
    memset(map->pml4, 0, 0x1000);
    map->pml4[1] = higher_half_pml4;

    return map;
}

void arch_paging_map_destroy(arch_paging_map_t *map)
{
    heap_free(map);
    // TODO: destroy page tables
}

// Map loading

void arch_paging_map_load(arch_paging_map_t *map)
{
    asm volatile(
        "msr ttbr0_el1, %0\n"
        "isb\n"
        :
        : "r"((uintptr_t)map->pml4[0] - HHDM)
        : "memory");

    // The kernel's higher half map only needs to be loaded once.
    if (!ttbr1_loaded)
    {
        asm volatile(
            "msr ttbr1_el1, %0\n"
            "isb\n"
            :
            : "r"((uintptr_t)map->pml4[1] - HHDM)
            : "memory");
        ttbr1_loaded = true;
    }
}

// Init

void arch_paging_init()
{
    higher_half_pml4 = (pte_t *)(pm_alloc(0)->addr + HHDM);
    memset(higher_half_pml4, 0, 0x1000);
}
