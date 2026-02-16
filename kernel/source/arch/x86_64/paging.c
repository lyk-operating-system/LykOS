#include "arch/paging.h"

#include "arch/x86_64/msr.h"
#include "assert.h"
#include "hhdm.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/pm.h"

#define PTE_PRESENT   (1ull <<  0)
#define PTE_WRITE     (1ull <<  1)
#define PTE_USER      (1ull <<  2)
#define PTE_ACCESSED  (1ull <<  5)
#define PTE_DIRTY     (1ull <<  6)
#define PTE_HUGE      (1ull <<  7)
#define PTE_GLOBAL    (1ull <<  8)
#define PTE_NX        (1ull << 63)

#define PTE_PAT_IDX(IDX) (IDX << 3)

#define PTE_ADDR_MASK(VALUE) ((VALUE) & 0x000FFFFFFFFFF000ull)

typedef uint64_t pte_t;

struct arch_paging_map
{
    pte_t *pml4;
};

// Mapping and unmapping

static inline void pt_children_inc(pte_t *table)
{
    page_t *p = pm_phys_to_page(((uintptr_t)table) - HHDM);
    atomic_fetch_add_explicit(&p->children, 1, memory_order_relaxed);
}

static inline bool pt_children_dec(pte_t *table)
{
    page_t *p = pm_phys_to_page(((uintptr_t)table) - HHDM);
    return atomic_fetch_sub_explicit(&p->children, 1, memory_order_relaxed) == 1;
}

static pte_t *get_next_level(pte_t *table, uint64_t idx, bool alloc, bool user)
{
    if (table[idx] & PTE_PRESENT)
        return (pte_t *)(PTE_ADDR_MASK(table[idx]) + HHDM);

    if (!alloc)
        return NULL;

    page_t *p = pm_alloc(0);
    if (!p)
        return NULL;

    uintptr_t phys = p->addr;
    pte_t *next_level = (pte_t *)(phys + HHDM);
    memset(next_level, 0, 0x1000);

    table[idx] = phys | PTE_PRESENT | PTE_WRITE | (user ? PTE_USER : 0);
    pt_children_inc(table);

    return next_level;
}

int arch_paging_map_page(arch_paging_map_t *map, uintptr_t vaddr, uintptr_t paddr, size_t size, vm_protection_t prot, vm_cache_t cache)
{
    ASSERT(size == ARCH_PAGE_SIZE_4K
        || size == ARCH_PAGE_SIZE_2M
        || size == ARCH_PAGE_SIZE_1G);
    ASSERT(vaddr % size == 0);
    ASSERT(paddr % size == 0);

    pte_t flags = 0;
    if (!prot.read) log(LOG_ERROR, "No-read mapping is not supported on x86_64!");
    if (prot.write) flags |= PTE_WRITE;
    if (!prot.exec) flags |= PTE_NX;
    const int attr_idx[] = {
        [VM_CACHE_STANDARD]      = 0,
        [VM_CACHE_WRITE_THROUGH] = 1,
        [VM_CACHE_WRITE_COMBINE] = 2,
        [VM_CACHE_NONE]          = 3,
    };
    flags |= PTE_PAT_IDX(attr_idx[cache]);

    bool is_user = vaddr < HHDM;

    size_t indices[] = {
        (vaddr >> 12) & 0x1FF,
        (vaddr >> 21) & 0x1FF,
        (vaddr >> 30) & 0x1FF,
        (vaddr >> 39) & 0x1FF
    };

    pte_t *table = map->pml4;
    size_t target_level = (size == 1 * GIB) ? 2
                        : (size == 2 * MIB) ? 1
                        : 0;
    for (size_t level = 3; level > target_level; level--)
    {
        size_t idx = indices[level];
        ASSERT(!(table[idx] & PTE_HUGE));

        pte_t *next = get_next_level(table, idx, true, is_user);
        if (!next)
            return -1;
        table = next;
    }

    // Leaf

    uint64_t leaf_idx = indices[target_level];
    ASSERT(!(table[leaf_idx] & PTE_PRESENT));

    pte_t entry = paddr | PTE_PRESENT | flags | (is_user ? PTE_USER : 0);
    if (target_level > 0)
        entry |= PTE_HUGE;
    pt_children_inc(table);

    table[leaf_idx] = entry;

    return 0;
}

int arch_paging_unmap_page(arch_paging_map_t *map, uintptr_t vaddr)
{
    size_t indices[] = {
        (vaddr >> 12) & 0x1FF, // PML1 entry
        (vaddr >> 21) & 0x1FF, // PML2 entry
        (vaddr >> 30) & 0x1FF, // PML3 entry
        (vaddr >> 39) & 0x1FF  // PML4 entry
    };

    pte_t *tables[4]; // Track visited tables to climb back up later.
    tables[3] = map->pml4;

    // Descend
    size_t level;
    for (level = 3; level >= 1; level--)
    {
        pte_t entry = tables[level][indices[level]];
        if (!(entry & PTE_PRESENT)) // Not mapped, nothing to do.
            return -1;
        if (entry & PTE_HUGE) // Huge Page, end walk early.
            break;

        tables[level - 1] = (pte_t *)(PTE_ADDR_MASK(entry) + HHDM);
    }

    // Clear the mapping
    size_t leaf_idx = indices[level];
    if (!(tables[level][leaf_idx] & PTE_PRESENT))
        return -1; // Not mapped, nothing to do.

    tables[level][leaf_idx] = 0;
    // Ascend
    while (level <= 3)
    {
        if (!pt_children_dec(tables[level]))
            break;

        uintptr_t phys = (uintptr_t)tables[level] - HHDM;

        // Disconnect from parent before freeing child
        level++;
        tables[level][indices[level]] = 0;

        pm_free(pm_phys_to_page(phys));
    }

    // Flush TLB
    asm volatile("invlpg (%0)" ::"r"(vaddr) : "memory");

    return 0;
}

// Flags

int arch_paging_prot_page(arch_paging_map_t *map, uintptr_t vaddr, size_t size, vm_protection_t prot)
{
    ASSERT(size == ARCH_PAGE_SIZE_4K
        || size == ARCH_PAGE_SIZE_2M
        || size == ARCH_PAGE_SIZE_1G);
    ASSERT(vaddr % size == 0);

    bool is_user = vaddr < HHDM;

    size_t indices[] = {
        (vaddr >> 12) & 0x1FF,
        (vaddr >> 21) & 0x1FF,
        (vaddr >> 30) & 0x1FF,
        (vaddr >> 39) & 0x1FF
    };

    pte_t *table = map->pml4;
    size_t target_level = (size == 1 * GIB) ? 2
                        : (size == 2 * MIB) ? 1
                        : 0;
    for (size_t level = 3; level > target_level; level--)
    {
        size_t idx = indices[level];
        ASSERT(table[idx] & PTE_PRESENT);
        ASSERT(!(table[idx] & PTE_HUGE));

        pte_t *next = get_next_level(table, idx, false, is_user);
        ASSERT(next);
        table = next;
    }

    // Leaf

    uint64_t leaf_idx = indices[target_level];
    ASSERT(table[leaf_idx] & PTE_PRESENT);

    pte_t entry = table[leaf_idx];
    entry &= ~(PTE_WRITE | PTE_NX | PTE_USER);
    if (!prot.read) log(LOG_ERROR, "No-read mapping is not supported on x86_64!");
    if (prot.write) entry |= PTE_WRITE;
    if (!prot.exec) entry |= PTE_NX;
    if (is_user)    entry |= PTE_USER;

    table[leaf_idx] = entry;

    return 0;
}

// Utils

bool arch_paging_vaddr_to_paddr(const arch_paging_map_t *map, uintptr_t vaddr, uintptr_t *out_paddr)
{
    uint64_t pml4e = (vaddr >> 39) & 0x1FF;
    uint64_t pml3e = (vaddr >> 30) & 0x1FF;
    uint64_t pml2e = (vaddr >> 21) & 0x1FF;
    uint64_t pml1e = (vaddr >> 12) & 0x1FF;

    pte_t pml4ent = map->pml4[pml4e];
    if (!(pml4ent & PTE_PRESENT))
        return false;

    pte_t *pml3 = (pte_t *)(PTE_ADDR_MASK(pml4ent) + HHDM);
    pte_t pml3ent = pml3[pml3e];
    if (!(pml3ent & PTE_PRESENT))
        return false;

    if (pml3ent & PTE_HUGE)
    {
        *out_paddr = PTE_ADDR_MASK(pml3ent) + (vaddr & ((1ull << 30) - 1));
        return true;
    }

    pte_t *pml2 = (pte_t *)(PTE_ADDR_MASK(pml3ent) + HHDM);
    pte_t pml2ent = pml2[pml2e];
    if (!(pml2ent & PTE_PRESENT))
        return false;

    if (pml2ent & PTE_HUGE)
    {
        *out_paddr = PTE_ADDR_MASK(pml2ent) + (vaddr & ((1ull << 21) - 1));
        return true;
    }

    pte_t *pml1 = (pte_t *)(PTE_ADDR_MASK(pml2ent) + HHDM);
    pte_t pml1ent = pml1[pml1e];
    if (!(pml1ent & PTE_PRESENT))
        return false;

    *out_paddr = PTE_ADDR_MASK(pml1ent) + (vaddr & 0xFFF);
    return true;
}


// Map creation and destruction

static pte_t higher_half_entries[256];

arch_paging_map_t *arch_paging_map_create()
{
    arch_paging_map_t *map = heap_alloc(sizeof(arch_paging_map_t));
    map->pml4 = (pte_t *)(pm_alloc(0)->addr + HHDM);
    memset(map->pml4, 0, 0x1000);

    for (int i = 0; i < 256; i++)
        map->pml4[i + 256] = higher_half_entries[i];

    return map;
}

static void delete_level(pte_t *level, int depth)
{
    if (depth != 1)
        for (size_t i = 0; i < 512; i++)
        {
            if (!(level[i] & PTE_PRESENT) || level[i] & PTE_HUGE)
                continue;

            delete_level((pte_t *)(PTE_ADDR_MASK(level[i]) + HHDM), depth - 1);
        }

    pm_free(pm_phys_to_page((uintptr_t)level - HHDM));
}

void arch_paging_map_destroy(arch_paging_map_t *map)
{
    delete_level(map->pml4, 4);
    heap_free(map);
}

// Map loading

void arch_paging_map_load(arch_paging_map_t *map)
{
    asm volatile("movq %0, %%cr3" :: "r"((uintptr_t)map->pml4 - HHDM) : "memory");
}

// Init

void arch_paging_init()
{
    for (int i = 0; i < 256; i++)
    {
        pte_t *pml3 = (pte_t *)(pm_alloc(0)->addr + HHDM);
        memset(pml3, 0, 0x1000);
        higher_half_entries[i] = (pte_t)((uintptr_t)pml3 - HHDM) | PTE_PRESENT | PTE_WRITE | PTE_USER;
    }

    // Setup PAT register
    uint64_t pat =  6ull         // Write-Back
                 | (4ull <<  8)  // Write-Through
                 | (1ull << 16)  // Write-Combining
                 | (0ull << 24); // Uncached
    x86_64_msr_write(X86_64_MSR_PAT, pat);
}
