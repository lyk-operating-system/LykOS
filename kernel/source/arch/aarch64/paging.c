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
    uint64_t l0e = (vaddr >> 39) & 0x1FF;
    uint64_t l1e = (vaddr >> 30) & 0x1FF;
    uint64_t l2e = (vaddr >> 21) & 0x1FF;
    uint64_t l3e = (vaddr >> 12) & 0x1FF;

    pte_t *l0 = map->pml4[vaddr >= HHDM ? 1 : 0];
    pte_t *l1;
    pte_t *l2;
    pte_t *l3;

    pte_t _prot = translate_prot(prot);

    // L0 -> L1
    if (l0[l0e] & PTE_VALID)
        l1 = (pte_t *)(PTE_ADDR_MASK(l0[l0e]) + HHDM);
    else
    {
        uintptr_t phys = pm_alloc(0)->addr;
        l1 = (pte_t *)(phys + HHDM);
        memset(l1, 0, 0x1000);
        l0[l0e] = phys | PTE_VALID | PTE_TABLE | PTE_ACCESS;
    }

    // 1 GiB block
    if (size == 1 * GIB)
    {
        l1[l1e] = paddr | PTE_VALID | PTE_ACCESS | _prot;
        return 0;
    }

    // L1 -> L2
    if (l1[l1e] & PTE_VALID)
        l2 = (pte_t *)(PTE_ADDR_MASK(l1[l1e]) + HHDM);
    else
    {
        uintptr_t phys = pm_alloc(0)->addr;
        l2 = (pte_t *)(phys + HHDM);
        memset(l2, 0, 0x1000);
        l1[l1e] = phys | PTE_VALID | PTE_TABLE | PTE_ACCESS;
    }

    // 2 MiB block
    if (size == 2 * MIB)
    {
        l2[l2e] = paddr | PTE_VALID | PTE_ACCESS | _prot;
        return 0;
    }

    // L2 -> L3
    if (l2[l2e] & PTE_VALID)
        l3 = (pte_t *)(PTE_ADDR_MASK(l2[l2e]) + HHDM);
    else
    {
        uintptr_t phys = pm_alloc(0)->addr;
        l3 = (pte_t *)(phys + HHDM);
        memset(l3, 0, 0x1000);
        l2[l2e] = phys | PTE_VALID | PTE_TABLE | PTE_ACCESS;
    }

    // 4 KiB page
    l3[l3e] = paddr | PTE_VALID | PTE_TABLE | PTE_ACCESS | _prot;
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
