#include "sys/syscall.h"

#include "fs/vfs.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "sync/spinlock.h"
#include "uapi/errno.h"
#include "utils/string.h"
#include <stdint.h>

sys_ret_t syscall_getcwd(const char *path, size_t size)
{
    static volatile spinlock_t cwd_lock = SPINLOCK_INIT;
    spinlock_acquire(&cwd_lock);

    const char *cwd = sys_curr_proc()->cwd;
    size_t len = strlen(cwd);

    if (len + 1 > size)
        return (sys_ret_t) {0, ERANGE};

    size_t copied = vm_copy_to_user(sys_curr_as(), (uintptr_t)path, (void *)cwd, len);
    if (copied != len + 1)
        return (sys_ret_t) {0, EFAULT};

    spinlock_release(&cwd_lock);
    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_chdir(const char *path)
{
    static volatile spinlock_t cwd_lock = SPINLOCK_INIT;
    spinlock_acquire(&cwd_lock);

    char kpath[PATH_MAX_NAME_LEN];
    size_t copied = vm_copy_from_user(sys_curr_as(), kpath, (uintptr_t)path, sizeof(kpath));
    if (copied == 0) return (sys_ret_t) {0, EFAULT};

    kpath[PATH_MAX_NAME_LEN - 1] = '\0';

    // check if dir exists
    vnode_t *vn;
    int ret = vfs_lookup(kpath, &vn);
    if (ret != EOK) return (sys_ret_t) {0, ret};

    // check if path is dir
    if (vn->type != VDIR)
    {
        vnode_unref(vn);
        return (sys_ret_t) {0, ENOTDIR};
    }
    vnode_unref(vn);

    // save cwd on heap for efficiency
    size_t path_len = strlen(kpath);
    char *new_cwd = heap_alloc(path_len + 1);
    if(!new_cwd) return (sys_ret_t) {0, ENOMEM};

    memcpy(new_cwd, kpath, path_len + 1);

    const char *old_cwd = sys_curr_proc()->cwd;
    sys_curr_proc()->cwd = new_cwd;

    if (old_cwd) heap_free((void *)old_cwd);

    spinlock_release(&cwd_lock);
    return (sys_ret_t) {0, EOK};
}

// --- TBD ---
// sys_ret_t sys_sync(void)
// {
//     return (sys_ret_t) {0, EOK};
// }

// sys_ret_t sys_statfs(const char *path, struct statfs *user_buf)
// {
//     return (sys_ret_t) {0, ENOSYS};
// }

// sys_ret_t sys_mount(const char *user_src,
//                     const char *user_dest,
//                     const char *user_fstype,
//                     unsigned int flags,
//                     void *data)
// {
//     return (sys_ret_t) {0, ENOTSUP};
// }

// ----

sys_ret_t syscall_mkdir(const char *path)
{
    char kpath[PATH_MAX_NAME_LEN];
    size_t copied = vm_copy_from_user(sys_curr_as(), kpath, (uintptr_t)path, sizeof(kpath));
    if (copied == 0) return (sys_ret_t) {31, EFAULT};

    kpath[PATH_MAX_NAME_LEN - 1] = '\0';

    vnode_t *vn;
    int ret = vfs_create(kpath, VDIR, &vn);
    if (ret != EOK) return (sys_ret_t) {0, ret};

    return (sys_ret_t) {0, EOK};
}

sys_ret_t syscall_rmdir(const char *path)
{
    char kpath[PATH_MAX_NAME_LEN];
    size_t copied = vm_copy_from_user(sys_curr_as(), kpath, (uintptr_t)path, sizeof(kpath));
    if (copied == 0) return (sys_ret_t) {31, EFAULT};

    kpath[PATH_MAX_NAME_LEN - 1] ='\0';

    vnode_t *vn;
    // verify if path is folder
    int ret = vfs_lookup(kpath, &vn);
    if (ret != EOK) return (sys_ret_t) {0, ret};

    if (vn->type != VDIR)
    {
        //vnode_unref(vn);
        return (sys_ret_t) {0, ENOTDIR};
    }
    vnode_unref(vn);

    ret = vfs_remove(kpath);
    if(ret != EOK) return (sys_ret_t) {0, ret};

    return (sys_ret_t) {0, EOK};
}
