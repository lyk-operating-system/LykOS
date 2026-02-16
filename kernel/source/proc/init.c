#include "proc/init.h"

#include "arch/types.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "mm/vm.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "uapi/errno.h"
#include "utils/elf.h"
#include "utils/math.h"
#include <stddef.h>
#include <stdint.h>

proc_t *init_load(vnode_t *file)
{
    log(LOG_INFO, "Loading init process...");

    // Variable to be used as output parameter for file read/write operations.
    uint64_t count;

    Elf64_Ehdr ehdr;
    if (vfs_read(file, &ehdr, 0, sizeof(Elf64_Ehdr), &count) != EOK
    ||  count != sizeof(Elf64_Ehdr))
    {
        log(LOG_ERROR, "Could not read file header!");
        return NULL;
    }

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

    proc_t *proc = proc_create(file->name, "/", true);

    CLEANUP Elf64_Phdr *ph_table = heap_alloc(ehdr.e_phentsize * ehdr.e_phnum);
    if (vfs_read(file, ph_table, ehdr.e_phoff, ehdr.e_phentsize * ehdr.e_phnum, &count) != EOK
    ||  count != ehdr.e_phentsize * ehdr.e_phnum)
    {
        log(LOG_ERROR, "Could not load the program headers!");
        return NULL;
    }

    for (size_t i = 0; i < ehdr.e_phnum; i++)
    {
        Elf64_Phdr *ph = &ph_table[i];

        CLEANUP uint8_t *buf = heap_alloc(1024);
        if (!buf)
        {
            log(LOG_ERROR, "Could not allocate a page for the file IO buffer.");
            return NULL;
        }

        if (ph->p_type == PT_LOAD && ph->p_memsz != 0)
        {
            uintptr_t start = FLOOR(ph->p_vaddr, ARCH_PAGE_GRAN);
            uintptr_t end   = CEIL(ph->p_vaddr + ph->p_memsz, ARCH_PAGE_GRAN);
            uint64_t  diff  = end - start;

            uintptr_t out;
            int err = vm_map(
                proc->as,
                start, diff,
                VM_PROTECTION_FULL,
                VM_MAP_ANON | VM_MAP_POPULATE | VM_MAP_FIXED | VM_MAP_PRIVATE,
                NULL, 0,
                &out
            );
            if (err != EOK || out != start)
            {
                log(LOG_ERROR, "Could not map the program headers!");
                return NULL;
            }

            if (ph->p_filesz == 0)
                continue;

            size_t read_bytes = 0;
            while (read_bytes < ph->p_filesz)
            {
                size_t to_copy = MIN(ph->p_filesz - read_bytes, ARCH_PAGE_GRAN);

                if (vfs_read(file, buf, ph->p_offset + read_bytes, to_copy, &count) != EOK
                ||  count != to_copy)
                {
                    log(LOG_ERROR, "Could not map the program headers!");
                    return NULL;
                }
                vm_copy_to_user(proc->as, ph->p_vaddr + read_bytes, buf, to_copy);

                read_bytes += to_copy;
            }
        }
    }

    thread_create(proc, ehdr.e_entry);
    return proc;
}
