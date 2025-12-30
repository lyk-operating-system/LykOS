#include "proc/exec.h"

#include "assert.h"
#include "hhdm.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "mm/pm.h"
#include "mm/vm.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "utils/elf.h"
#include "utils/math.h"
#include <stddef.h>
#include <stdint.h>

int exec_load(vnode_t *file, proc_t **out)
{
    log(LOG_INFO, "Loading executable `%s`.", file->name);

    // Variable to be used as output parameter for file read/write operations.
    uint64_t count;

    Elf64_Ehdr ehdr;
    if (vfs_read(file, &ehdr, sizeof(Elf64_Ehdr), 0, &count) != EOK
    ||  count != sizeof(Elf64_Ehdr))
        log(LOG_ERROR, "Could not read file header!");

    if (memcmp(ehdr.e_ident, "\x7F""ELF", 4)
    ||  ehdr.e_ident[EI_CLASS]   != ELFCLASS64
    ||  ehdr.e_ident[EI_DATA]    != ELFDATA2LSB
    #if defined(__x86_64__)
    ||  ehdr.e_machine           != EM_x86_64
    #elif defined(__aarch64__)
    ||  ehdr.e_machine           != EM_AARCH64
    #endif
    ||  ehdr.e_ident[EI_VERSION] != EV_CURRENT
    ||  ehdr.e_type              != ET_EXEC)
        log(LOG_ERROR, "Incompatible ELF file `%s`!", file->name);

    proc_t *proc = proc_create(file->name, true);

    CLEANUP Elf64_Phdr *ph_table = heap_alloc(ehdr.e_phentsize * ehdr.e_phnum);
    if (vfs_read(file, ph_table, ehdr.e_phentsize * ehdr.e_phnum, ehdr.e_phoff, &count) != EOK
    ||  count != ehdr.e_phentsize * ehdr.e_phnum)
    {
        // TODO: cleanup
        log(LOG_ERROR, "Could not load the program headers!");
    }

    for (size_t i = 0; i < ehdr.e_phnum; i++)
    {
        Elf64_Phdr *ph = &ph_table[i];

        if (ph->p_type == PT_LOAD && ph->p_memsz != 0)
        {
            uintptr_t start = FLOOR(ph->p_vaddr, ARCH_PAGE_GRAN);
            uintptr_t end   = CEIL(ph->p_vaddr + ph->p_memsz, ARCH_PAGE_GRAN);
            uint64_t  diff  = end - start;

            uintptr_t out;
            int err = vm_map_vnode(
                proc->as,
                start,
                diff,
                MM_PROT_FULL,
                VM_MAP_ANON | VM_MAP_POPULATE | VM_MAP_FIXED | VM_MAP_PRIVATE,
                NULL,
                0,
                &out
            );

            if (err != EOK || out != start)
            {
                // TODO: delete proc
                log(LOG_ERROR, "Could not map the program headers!");
                return err;
            }

            // TODO: cleanup
            ASSERT(pm_order_to_pagecount(10) * ARCH_PAGE_GRAN >= ph->p_filesz);
            uintptr_t buf = pm_alloc(10);

            if (vfs_read(file, (void*)(buf + HHDM), ph->p_filesz, ph->p_offset, &count) != EOK
            || count != ph->p_filesz)
            {
                // TODO: cleanup
                log(LOG_ERROR, "Could not map the program headers!");
                return err;
            }
            vm_copy_to(proc->as, ph->p_vaddr, buf + HHDM, ph->p_filesz);

            pm_free(buf);
        }
    }

    thread_create(proc, ehdr.e_entry);
    *out = proc;
    return EOK;
}
