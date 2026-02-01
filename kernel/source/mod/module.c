#include "mod/module.h"

#include "arch/types.h"
#include "hhdm.h"
#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "mm/vm.h"
#include "mod/ksym.h"
#include "uapi/errno.h"
#include "utils/elf.h"
#include "utils/math.h"
#include "utils/string.h"

static void module_fetch_modinfo(module_t *mod, const char *sym_name, uintptr_t sym_val)
{
#define MATCH(X) strcmp(sym_name, X) == 0

    if      (MATCH("__module_install")    ) mod->install     = (void(*)())sym_val;
    else if (MATCH("__module_destroy")    ) mod->destroy     = (void(*)())sym_val;
    else if (MATCH("__module_name")       ) mod->name        = (const char *)sym_val;
    else if (MATCH("__module_version")    ) mod->version     = (const char *)sym_val;
    else if (MATCH("__module_description")) mod->description = (const char *)sym_val;
    else if (MATCH("__module_author")     ) mod->author      = (const char *)sym_val;

#undef MATCH
}

int module_load(vnode_t *file, module_t **out)
{
    if (!file || !out)
        return EINVAL;

    log(LOG_INFO, "Loading module `%s`.", file->name);
    module_t module;
    memset(&module, 0, sizeof(module_t));

    // Variable to be used as output parameter for file read/write operations.
    uint64_t count;

    Elf64_Ehdr ehdr;
    if (vfs_read(file, &ehdr, 0, sizeof(Elf64_Ehdr), &count) != EOK
    ||  count != sizeof(Elf64_Ehdr))
    {
        log(LOG_ERROR, "Could not read file header!");
        return ENOEXEC;
    };

    // Check for compatibility.
    if (memcmp(ehdr.e_ident, "\x7F""ELF", 4)    ||
        ehdr.e_ident[EI_CLASS]   != ELFCLASS64  ||
        ehdr.e_ident[EI_DATA]    != ELFDATA2LSB ||
    #if defined(__x86_64__)
        ehdr.e_machine           != EM_x86_64   ||
    #elif defined(__aarch64__)
        ehdr.e_machine           != EM_AARCH64  ||
    #endif
        ehdr.e_ident[EI_VERSION] != EV_CURRENT  ||
        ehdr.e_type              != ET_REL)
    {
        return ENOEXEC;
    }

    // Allocate memory for the sections and save the address.
      Elf64_Shdr *shdr = vm_alloc(ehdr.e_shnum * sizeof(Elf64_Shdr));
      uintptr_t *section_addr = vm_alloc(ehdr.e_shnum * sizeof(uintptr_t));

    for(int i = 0; i < ehdr.e_shnum; i++)
    {
        if(vfs_read(file, &shdr[i], ehdr.e_shoff + (ehdr.e_shentsize * i), sizeof(Elf64_Shdr), &count) != EOK
        || count != sizeof(Elf64_Shdr))
        {
            log(LOG_ERROR, "Could not load section header list from file!");
            return ENOEXEC;
        }
        Elf64_Shdr *section = &shdr[i];

        if (section->sh_size == 0 || (section->sh_flags & SHF_ALLOC) == 0)
            continue;

        if (section->sh_type == SHT_PROGBITS)
        {
            uint64_t size = CEIL(section->sh_size, ARCH_PAGE_GRAN);
            uintptr_t mem;
            vm_map(
                vm_kernel_as,
                0,
                size,
                MM_PROT_WRITE | MM_PROT_EXEC,
                VM_MAP_ANON,
                NULL,
                0,
                &mem
            );
            if (vfs_read(file, (void *)mem, section->sh_offset, section->sh_size, &count) != EOK
            ||  count != section->sh_size)
            {
                log(LOG_ERROR, "Could not load section header from file!");
                return ENOEXEC;
            }
            section_addr[i] = mem;
        }
        else if (section->sh_type == SHT_NOBITS) // Global data.
        {
            uint64_t size = CEIL(section->sh_size, ARCH_PAGE_GRAN);
            uintptr_t mem;
            vm_map(
                vm_kernel_as,
                0,
                size,
                MM_PROT_WRITE | MM_PROT_EXEC,
                VM_MAP_ANON,
                NULL,
                0,
                &mem
            );

            section_addr[i] = mem;
        }
    }

    // Symbol table.
    Elf64_Shdr *symtab_hdr = NULL;
    for (int i = 0; i < ehdr.e_shnum; i++)
        if (shdr[i].sh_type == SHT_SYMTAB)
            symtab_hdr = &shdr[i];
    if (!symtab_hdr)
    {
        log(LOG_ERROR, "Missing symbol table!");
        return ENOEXEC;
    }
      void *symtab = vm_alloc(symtab_hdr->sh_size);
    if (vfs_read(file, symtab, symtab_hdr->sh_offset, symtab_hdr->sh_size, &count) != EOK || count != symtab_hdr->sh_size)
    {
        log(LOG_ERROR, "Could not load symbol table from file!");
        return ENOEXEC;
    }

    // String table.
    Elf64_Shdr *strtab_hdr = &shdr[symtab_hdr->sh_link];
    if (!strtab_hdr)
    {
        log(LOG_ERROR, "Missing string table!");
        return ENOEXEC;
    }
      char *strtab = vm_alloc(strtab_hdr->sh_size);
    if(vfs_read(file, strtab, strtab_hdr->sh_offset, strtab_hdr->sh_size, &count) != EOK || count != strtab_hdr->sh_size)
    {
        log(LOG_ERROR, "Could not load string table from file!");
        return ENOEXEC;
    }

    // Resolve symbols.
    size_t sym_count = symtab_hdr->sh_size / symtab_hdr->sh_entsize;
    for (size_t i = 1; i < sym_count; i++)
    {
        Elf64_Sym  *sym  = (Elf64_Sym *)(symtab + (i * symtab_hdr->sh_entsize));
        const char *name = &strtab[sym->st_name];

        switch (sym->st_shndx)
        {
        case SHN_UNDEF:
            sym->st_value = ksym_resolve_symbol(name);
            if (sym->st_value == 0)
            {
                log(LOG_ERROR, "Symbol `%s` could not be resolved!", name);
                return ENOEXEC;
            }
            break;
        case SHN_ABS:
            break;
        case SHN_COMMON:
            log(LOG_WARN, "Unexpected common symbol.");
            break;
        default:
            sym->st_value += section_addr[sym->st_shndx];
            module_fetch_modinfo(&module, name, sym->st_value);
            break;
        }
    }

    if (module.install == NULL || module.destroy == NULL)
    {
        log(LOG_ERROR, "Module `%s` does not implement required functions.", file->name);
        return ENOEXEC;
    }

    // Load relocation sections.
    for (size_t i = 0; i < ehdr.e_shnum; i++)
    {
        Elf64_Shdr *section = &shdr[i];

        if (section->sh_type != SHT_RELA)
            continue;

          Elf64_Rela *rela_entries = vm_alloc(section->sh_size);
        if (vfs_read(file, rela_entries, section->sh_offset, section->sh_size, &count) != EOK
        ||  count != section->sh_size)
        {
            log(LOG_ERROR, "Could not load relocation entries from file!");
            return ENOEXEC;
        }

        for (size_t j = 0; j < section->sh_size / section->sh_entsize; j++)
        {
            Elf64_Rela *rela = &rela_entries[j];
            Elf64_Sym  *sym  = (Elf64_Sym *)(symtab + (ELF64_R_SYM(rela->r_info) * symtab_hdr->sh_entsize));

            void *addr = (void *)(section_addr[section->sh_info] + rela->r_offset);
            uintptr_t value = sym->st_value + rela->r_addend;
            uint64_t reloc_size;

            switch (ELF64_R_TYPE(rela->r_info))
            {
                case R_X86_64_64:
                    reloc_size = 8;
                    break;
                case R_X86_64_PC32:
                case R_X86_64_PLT32:
                    value -= (uintptr_t)addr;
                    reloc_size = 4;
                    break;
                case R_X86_64_32:
                case R_X86_64_32S:
                    reloc_size = 4;
                    break;
                case R_X86_64_PC64:
                    value -= (uintptr_t)addr;
                    reloc_size = 8;
                    break;
                default:
                    log(LOG_ERROR, "Unsupported relocation type: 0x%x.", ELF64_R_TYPE(rela->r_info));
                    return ENOEXEC;
            }

            memcpy(addr, &value, reloc_size);
        }
    }

    // TODO: clean the actual segments allocated for the module.

    *out = vm_alloc(sizeof(module_t));
    **out = module;

    log(LOG_INFO, "Kernel module loaded successfully.");
    return EOK;
}
