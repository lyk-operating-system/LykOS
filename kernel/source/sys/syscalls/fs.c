// #include "sys/syscall.h"

// #include "fs/vfs.h"
// #include "uapi/errno.h"
// #include "utils/string.h"

// sys_ret_t sys_getcwd(char *path, size_t size)
// {
//     const char *cwd = sys_curr_proc()->cwd;
//     size_t len = strlen(cwd);

//     vm_copy_to_user(sys_curr_as(), (uintptr_t)path, (void *)cwd, len);

//     return (sys_ret_t) {0, EOK};
// }

// sys_ret_t sys_chdir(const char *path)
// {
//     char kpath[PATH_MAX];
//     if (copy_from_user(kpath, path, sizeof(kpath)) != 0)
//         return -EFAULT;

//     vnode_t *vn;
//     int ret = vfs_lookup(kpath, 0, &vn);
//     if (ret != 0)
//         return ret;

//     if (vn->type != VNODE_DIR)
//         return -ENOTDIR;

//     current_proc()->cwd = vn;
//     return (sys_ret_t) {0, EOK};
// }

// sys_ret_t sys_sync(void)
// {
//     return (sys_ret_t) {0, ENOSYS};
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
//     return (sys_ret_t) {0, ENOSYS};
// }

// sys_ret_t sys_mkdir(const char *user_path)
// {
//     char kpath[PATH_MAX];
//     if (copy_from_user(kpath, user_path, sizeof(kpath)) != 0)
//         return -EFAULT;

//     vnode_t *parent;
//     const char *name;

//     // TODO: split path → parent vnode + name
//     return (sys_ret_t) {0, ENOSYS};
// }

// sys_ret_t sys_rmdir(const char *user_path)
// {
//     char kpath[PATH_MAX];
//     if (copy_from_user(kpath, user_path, sizeof(kpath)) != 0)
//         return -EFAULT;

//     vnode_t *parent;
//     const char *name;

//     // TODO: split path → parent vnode + name
//     return (sys_ret_t) {0, EOK};
// }
