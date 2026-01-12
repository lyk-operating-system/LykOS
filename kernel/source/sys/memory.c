#include "sys/syscall.h"

#include "fs/vfs.h"
#include "log.h"
#include "mm/mm.h"
#include "proc/proc.h"
#include "proc/sched.h"
#include "proc/thread.h"
#include "uapi/errno.h"
#include "utils/math.h"

#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02
#define PROT_EXEC  0x04

#define MAP_FAILED   ((void *)(-1))
#define MAP_FILE     0x00
#define MAP_SHARED   0x01
#define MAP_PRIVATE  0x02
#define MAP_FIXED    0x10
#define MAP_ANON     0x20

sys_ret_t syscall_mmap(uintptr_t addr, size_t length, int prot, int flags, int fd, size_t offset)
{
    proc_t *proc = sched_get_curr_thread()->owner;
    vm_addrspace_t *as = proc->as;

    size_t value, err;
    err = vm_map(as, addr, length, MM_PROT_WRITE | MM_PROT_USER, VM_MAP_ANON | VM_MAP_PRIVATE, NULL, 0, &value);

    return (sys_ret_t) {
        value,
        err
    };
}
