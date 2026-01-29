#include "assert.h"
#include "dev/device.h"
#include "dev/bus.h"
#include "fs/devfs.h"
#include "gfx/simplefb.h"
#include "mm/mm.h"
#include "uapi/errno.h"

static struct
{
    uint32_t *address;
    size_t size;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
}
fb_info;

static int read(vnode_t *vn, void *buffer, uint64_t offset, uint64_t count,
                uint64_t *out_bytes_read)
{
    ASSERT(buffer && out_bytes_read);

    if (offset >= fb_info.size)
    {
        *out_bytes_read = 0;
        return EOK;
    }

    size_t available = fb_info.size - offset;
    size_t to_read = (count < available) ? count : available;

    memcpy(buffer, (uint8_t *)fb_info.address + offset, to_read);

    *out_bytes_read = to_read;
    return EOK;
}

static int write(vnode_t *vn, const void *buffer, uint64_t offset, uint64_t count,
                 uint64_t *out_bytes_written)
{
    ASSERT(buffer && out_bytes_written);

    if (offset >= fb_info.size)
    {
        *out_bytes_written = 0;
        return EOK;
    }

    size_t available = fb_info.size - offset;
    size_t to_write = (count < available) ? count : available;

    memcpy((uint8_t *)fb_info.address + offset, buffer, to_write);

    *out_bytes_written = to_write;
    return EOK;
}

static int mmap(vnode_t *vn, vm_addrspace_t *as, uintptr_t vaddr, size_t length,
            int prot, int flags, uint64_t offset)
{
    return ENOTSUP;
}

static vnode_ops_t file_ops = {
    .read = read,
    .write = write,
    .mmap = mmap
};

static device_t fb_device_t = {
    .name = "UEFI GOP Framebuffer",
    .class = DEVICE_DISPLAY,
    .power_ops = NULL,
};

void virtual_fb_init()
{
    fb_info.address = (uint32_t *)simplefb_addr;
    fb_info.height = simplefb_height;
    fb_info.width = simplefb_width;
    fb_info.pitch = simplefb_pitch;
    fb_info.size = simplefb_size;

    bus_t *virtual_bus = bus_get("virtual");
    ASSERT(virtual_bus);

    bool ret = false;
    ret = virtual_bus->register_device(&fb_device_t);
    ASSERT(ret);
    ret = devfs_register_device("/dev/fb0", VCHR, &file_ops, NULL);
    ASSERT(ret);

    bus_put(virtual_bus);
}
