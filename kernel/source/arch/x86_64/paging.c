#include "arch/paging.h"

#include "assert.h"
#include "hhdm.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "mm/pm.h"

#define PTE_PRESENT   (1ull <<  0)
#define PTE_WRITE     (1ull <<  1)
#define PTE_USER      (1ull <<  2)
#define PTE_HUGE      (1ull <<  7)
#define PTE_GLOBAL    (1ull <<  8)
#define PTE_NX        (1ull << 63)

#define PTE_ADDR_MASK(VALUE) ((VALUE) & 0x000FFFFFFFFFF000ull)

typedef uint64_t pte_t;

struct arch_paging_map
{
    pte_t *pml4;
};

// Helpers

static int translate_prot(int prot)
{
    uint64_t pteprot = 0;

    if (prot & MM_PROT_WRITE)
        pteprot |= PTE_WRITE;
    if (prot & MM_PROT_USER)
        pteprot |= PTE_USER;
    if (!(prot & MM_PROT_EXEC))
        pteprot |= PTE_NX;

    return pteprot;
}

// Mapping and unmapping

static inline uint64_t hh_user_flag(bool hh)
{
    return hh ? 0 : PTE_USER;
}

static inline uint64_t hh_leaf_flags(bool hh)
{
    return hh ? 0 : (PTE_USER | PTE_GLOBAL);
}

int arch_paging_map_page(arch_paging_map_t *map, uintptr_t vaddr, uintptr_t paddr, size_t size, int prot)
{
    uint64_t pml4e = (vaddr >> 39) & 0x1FF;
    uint64_t pml3e = (vaddr >> 30) & 0x1FF;
    uint64_t pml2e = (vaddr >> 21) & 0x1FF;
    uint64_t pml1e = (vaddr >> 12) & 0x1FF;

    pte_t *pml4 = map->pml4, *pml3, *pml2, *pml1;

    pte_t _prot = translate_prot(prot);
    bool hh = vaddr >= HHDM; // Is higher half?

    // User perms must be set only in lower half.
    ASSERT(hh ? !(_prot & PTE_USER) : (_prot & PTE_USER))

    // PML4 -> PML3
    if (pml4[pml4e] & PTE_PRESENT)
        pml3 = (pte_t *)(PTE_ADDR_MASK(pml4[pml4e]) + HHDM);
    else
    {
        uintptr_t phys = pm_alloc(0)->addr;
        pml3 = (pte_t *)(phys + HHDM);
        memset(pml3, 0, 0x1000);
        pml4[pml4e] = phys | PTE_PRESENT | PTE_WRITE | hh_user_flag(hh);
    }

    // 1 GiB page
    if (size == 1 * GIB)
    {
        pml3[pml3e] = paddr | _prot | PTE_PRESENT | PTE_HUGE | hh_leaf_flags(hh);
        return 0;
    }

    // PML3 -> PML2
    if (pml3[pml3e] & PTE_PRESENT)
        pml2 = (pte_t *)(PTE_ADDR_MASK(pml3[pml3e]) + HHDM);
    else
    {
        uintptr_t phys = pm_alloc(0)->addr;
        pml2 = (pte_t *)(phys + HHDM);
        memset(pml2, 0, 0x1000);
        pml3[pml3e] = phys | PTE_PRESENT | PTE_WRITE | hh_user_flag(hh);
    }

    // 2 MiB page
    if (size == 2 * MIB)
    {
        pml2[pml2e] = paddr | _prot | PTE_PRESENT | PTE_HUGE | hh_leaf_flags(hh);
        return 0;
    }

    // PML2 -> PML1
    if (pml2[pml2e] & PTE_PRESENT)
        pml1 = (pte_t *)(PTE_ADDR_MASK(pml2[pml2e]) + HHDM);
    else
    {
        uintptr_t phys = pm_alloc(0)->addr;
        pml1 = (pte_t *)(phys + HHDM);
        memset(pml1, 0, 0x1000);
        pml2[pml2e] = phys | PTE_PRESENT | PTE_WRITE | hh_user_flag(hh);
    }

    // 4 KiB page
    pml1[pml1e] = paddr | _prot | PTE_PRESENT | hh_leaf_flags(hh);
    return 0;
}

// Utils

bool arch_paging_vaddr_to_paddr(arch_paging_map_t *map, uintptr_t vaddr, uintptr_t *out_paddr)
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
}
