#include "fs/ustar.h"

#include <stddef.h>
#include <stdint.h>

#include "log.h"
#include "panic.h"
#include "mm/heap.h"
#include "mm/mm.h"

#include "utils/printf.h"
#include "utils/string.h"

#include "fs/ramfs.h"
#include "fs/vfs.h"

#define USTAR_BLOCK_SIZE 512

// Helpers
static uint64_t ustar_parse_octal(const char *str, size_t len)
{
    uint64_t result = 0;
    for (size_t i = 0; i < len && str[i] >= '0' && str[i] <= '7'; i++)
        result = (result << 3) + (str[i] - '0');
    return result;
}

static size_t ustar_get_size(const ustar_header_t *header)
{
    return (size_t)ustar_parse_octal(header->size, sizeof(header->size));
}

static int ustar_validate_checksum(const ustar_header_t *header)
{
    const unsigned char *bytes = (const unsigned char *)header;
    unsigned int sum = 0;
    unsigned int stored = (unsigned int)ustar_parse_octal(header->checksum, sizeof(header->checksum));

    for (size_t i = 0; i < sizeof(ustar_header_t); i++)
    {
        if (i >= offsetof(ustar_header_t, checksum)
        &&  i < offsetof(ustar_header_t, checksum) + sizeof(header->checksum))
            sum += ' ';
        else
            sum += bytes[i];
    }

    return sum == stored;
}

int ustar_extract(const void *archive, uint64_t archive_size, const char *dest_path)
{
    if (!archive || !dest_path)
        return EINVAL;

    vnode_t *dest_vn;
    if (vfs_lookup(dest_path, &dest_vn) != EOK)
        panic("USTAR: destination path not found");

    const uint8_t *data = (const uint8_t *)archive;
    uint64_t offset = 0;

    while (offset + USTAR_BLOCK_SIZE <= archive_size)
    {
        const ustar_header_t *header = (const ustar_header_t *)(data + offset);

        if (header->name[0] == '\0') break; // check for archive end

        if (strncmp(header->magic, "ustar", 5) != 0) // check magic
        {
            offset += USTAR_BLOCK_SIZE;
            continue;
        }

        if (!ustar_validate_checksum(header))
        {
            offset += USTAR_BLOCK_SIZE;
            continue;
        }

        size_t file_size = ustar_get_size(header);
        offset += USTAR_BLOCK_SIZE;

        // build dest_path + archive
        char entry_path[PATH_MAX_NAME_LEN];
        if (header->prefix[0] != '\0')
            snprintf(entry_path, sizeof(entry_path), "%s%s", header->prefix, header->name);
        else
            snprintf(entry_path, sizeof(entry_path), "%s", header->name);

        char full_path[PATH_MAX_NAME_LEN];
        size_t base_len = strlen(dest_path);
        int needs_slash = base_len && dest_path[base_len - 1] != '/';
        snprintf(full_path, sizeof(full_path), "%s%s%s", dest_path, needs_slash ? "/" : "", entry_path);

        switch (header->typeflag)
        {
            case USTAR_DIRECTORY:
            {
                vnode_t *dir_vn = NULL;
                int ret = vfs_create(full_path, VDIR, &dir_vn);
                if (ret != EOK && ret != EEXIST)
                    log(LOG_ERROR, "USTAR: failed to create directory %s", full_path);
                break;
            }

            case USTAR_REGULAR:
            {
                vnode_t *file_vn = NULL;
                int ret = vfs_create(full_path, VREG, &file_vn);
                if (ret == EEXIST)
                    ret = vfs_lookup(full_path, &file_vn);
                if (ret != EOK)
                {
                    log(LOG_ERROR, "USTAR: failed to create file %s", full_path);
                    break;
                }

                if (file_vn && file_size > 0)
                {
                    uint64_t written;
                    if (vfs_write(file_vn, (void *)(data + offset), 0, file_size, &written) != EOK
                    ||  written != file_size)
                        log(LOG_ERROR, "USTAR: failed to write to created file %s", full_path);
                }
                break;
            }

            default:
                break;
        }

        uint64_t blocks = (file_size + USTAR_BLOCK_SIZE - 1) / USTAR_BLOCK_SIZE;
        offset += blocks * USTAR_BLOCK_SIZE;
    }

    log(LOG_INFO, "Loaded archive into filesystem");
    return EOK;
}
