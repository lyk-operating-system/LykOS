#include "arch/paging.h"

#include "assert.h"
#include "hhdm.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/pm.h"

#define PTE_VALID       (1ull <<  0)
#define PTE_TABLE       (1ull <<  1)
#define PTE_BLOCK       (0ull <<  1)
#define PTE_PAGE_4K     (1ull <<  1)
#define PTE_READONLY    (1ull <<  6)
#define PTE_USER        (1ull <<  7)
#define PTE_ACCESS      (1ull << 10)
#define PTE_XN          (1ull << 54)

#define PTE_ATTR_IDX(IDX) ((IDX) << 2)

#define PTE_ADDR_MASK(VALUE) ((VALUE) & 0x000FFFFFFFFFF000ull)

typedef uint64_t pte_t;

struct arch_paging_map
{
    pte_t *pml4[2];
};

// Mapping and unmapping

static pte_t *get_next_level(pte_t *table, uint64_t idx, bool alloc, bool user)
{
    if (table[idx] & PTE_VALID)
        return (pte_t *)(PTE_ADDR_MASK(table[idx]) + HHDM);

    if (!alloc)
        return NULL;

    page_t *p = pm_alloc(0);
    if (!p)
        return NULL;

    uintptr_t phys = p->addr;
    pte_t *next_level = (pte_t *)(phys + HHDM);
    memset(next_level, 0, 0x1000);

    table[idx] = phys | PTE_VALID | (user ? PTE_USER : 0);
    pm_page_refcount_inc(pm_phys_to_page(PTE_ADDR_MASK((uintptr_t)table - HHDM)));

    return next_level;
}

int arch_paging_map_page(arch_paging_map_t *map, uintptr_t vaddr, uintptr_t paddr, size_t size, vm_protection_t prot, vm_cache_t cache)
{
    ASSERT(size == ARCH_PAGE_SIZE_4K
        || size == ARCH_PAGE_SIZE_2M
        || size == ARCH_PAGE_SIZE_1G);
    ASSERT(vaddr % size == 0);
    ASSERT(paddr % size == 0);

    pte_t _prot = 0;
    if (!prot.read) log(LOG_ERROR, "No-read mapping is not supported on x86_64!");
    if (!prot.write) _prot |= PTE_READONLY;
    if (!prot.exec) _prot |= PTE_XN;
    const int attr_idx[] = {
        [VM_CACHE_STANDARD]      = 0,
        [VM_CACHE_WRITE_THROUGH] = 1,
        [VM_CACHE_WRITE_COMBINE] = 2,
        [VM_CACHE_NONE]          = 3,
    };
    _prot |= PTE_ATTR_IDX(attr_idx[cache]);

    bool is_user = vaddr < HHDM;
    pte_t *table = map->pml4[is_user ? 0 : 1];

    size_t indices[] = {
        (vaddr >> 39) & 0x1FF, // Level 0
        (vaddr >> 30) & 0x1FF, // Level 1
        (vaddr >> 21) & 0x1FF, // Level 2
        (vaddr >> 12) & 0x1FF  // Level 3
    };

    size_t target_level = (size == 1 * GIB) ? 1
                        : (size == 2 * MIB) ? 2
                        : 3;
    for (size_t level = 0; level < target_level; level++)
    {
        size_t idx = indices[level];
        ASSERT(table[idx] & PTE_TABLE);

        pte_t *next = get_next_level(table, idx, true, is_user);
        if (!next)
            return -1;
        table = next;
    }

    size_t leaf_idx = indices[target_level];
    ASSERT(!(table[leaf_idx] & PTE_VALID));

    pm_page_refcount_inc(pm_phys_to_page(PTE_ADDR_MASK((uintptr_t)table - HHDM)));
    pte_t entry = paddr | PTE_VALID | _prot | PTE_ACCESS | (is_user ? PTE_USER : 0);
    entry |= (target_level == 3) ? PTE_PAGE_4K : PTE_BLOCK;

    table[leaf_idx] = entry;

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

bool arch_paging_vaddr_to_paddr(const arch_paging_map_t *map, uintptr_t vaddr, uintptr_t *out_paddr)
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
        : "memory"
    );

    // The kernel's higher half map only needs to be loaded once.
    if (!ttbr1_loaded)
    {
        asm volatile(
            "msr ttbr1_el1, %0\n"
            "isb\n"
            :
            : "r"((uintptr_t)map->pml4[1] - HHDM)
            : "memory"
        );
        ttbr1_loaded = true;
    }
}

// Init

void arch_paging_init()
{
    higher_half_pml4 = (pte_t *)(pm_alloc(0)->addr + HHDM);
    memset(higher_half_pml4, 0, 0x1000);

    // Setup MAIR register
    uint64_t mair =  0b11111111ull         // Write-Back
                  | (0b10111011ull <<  8)  // Write-Through
                  | (0b00001100ull << 16)  // Write-Combining: GRE
                  | (0b00000000ull << 24); // Uncached: nGnRnE
    asm volatile ("msr mair_el1, %0" : : "r"(mair));
    asm volatile ("dsb ish");
    asm volatile ("isb");
}
