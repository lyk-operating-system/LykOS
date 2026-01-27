#include "bootreq.h"
#include "fs/ustar.h"
#include "fs/vfs.h"
#include "log.h"
#include "mod/ksym.h"
#include "mod/module.h"
#include "panic.h"
#include "proc/init.h"
#include "proc/sched.h"
#include "proc/smp.h"
#include "uapi/errno.h"
#include "utils/string.h"
#include <stddef.h>
#include <stdint.h>

static void load_initrd()
{
    if (bootreq_module.response == NULL)
        panic("Invalid bootloader module response provided by the bootloader!");
    for (size_t i = 0; i < bootreq_module.response->module_count; i++)
    {
        if (strcmp(bootreq_module.response->modules[i]->path, "/initrd.tar") == 0)
        {
            vnode_t *root;
            if (vfs_lookup("/", &root) != EOK)
            {
                log(LOG_FATAL, "Root fs node doesnt exist");
            }

            ustar_extract(
                bootreq_module.response->modules[i]->address,
                bootreq_module.response->modules[i]->size,
                "/"
            );
            break;
        }
    }
}

static void load_boot_modules()
{
    ksym_init();

    vnode_t *boot_module_dir;

    if (vfs_lookup("/boot/modules", &boot_module_dir) != EOK
    ||  boot_module_dir->type != VDIR)
    {
        log(LOG_INFO, "No boot modules directory found.");
        return;
    }
    // TO-DO: free entries
    vfs_dirent_t *entries;
    size_t entry_count;
    boot_module_dir->ops->readdir(boot_module_dir, &entries, &entry_count);
    for (size_t i = 0; i < entry_count; i++)
    {
        vnode_t *module_vn;
        if(boot_module_dir->ops->lookup(boot_module_dir, entries[i].name, &module_vn) != EOK
        || module_vn->type != VREG)
            continue;

        module_t *mod;
        if (module_load(module_vn, &mod) == EOK)
            mod->install();
        // TO-DO: closing
    }
}

static void load_init_proc()
{
    vnode_t *init_elf_file;
    if (vfs_lookup("/boot/init", &init_elf_file) != EOK
    ||  init_elf_file->type != VREG)
        panic("Init process not found!");

    proc_t *init_proc = init_load(init_elf_file);
    if (init_proc)
        sched_enqueue(LIST_GET_CONTAINER(init_proc->threads.head, thread_t, proc_thread_list_node));
    else
        panic("Failed to load init process!");
}


void kernel_main()
{
    vfs_init();

    load_initrd();
    load_boot_modules();
    load_init_proc();

    // Start other CPU cores and scheduler

    smp_init();

    while (true)
        ;
}
