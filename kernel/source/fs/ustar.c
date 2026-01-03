#include "fs/ustar.h"

#include <stddef.h>
#include <stdint.h>

#include "log.h"
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

// Main logic to load archive
static vnode_t *create_path(vnode_t *root, const char *path, int is_dir)
{
    if (!path || !*path) return root;

    char *path_copy = strdup(path);
    char *saveptr;
    char *token = strtok_r(path_copy, "/", &saveptr);
    char current_path[PATH_MAX_NAME_LEN] = "";
    vnode_t *current = NULL;

    while (token)
    {
        char *next_token = strtok_r(NULL, "/", &saveptr);
        int is_last = (next_token == NULL);

        if (strcmp(current_path, "/") != 0)
            strcat(current_path, "/");
        strcat(current_path, token);

        vnode_type_t type;
        if (is_last && !is_dir)
            type = VREG;
        else
            type = VDIR;

        if (vfs_lookup(current_path, &current) != EOK)
            if (vfs_create(current_path, type, &current) != EOK)
            {
                heap_free(path_copy);
                return NULL;
            }

        token = next_token;
    }

    heap_free(path_copy);
    return current;
}

int ustar_extract(const void *archive, uint64_t archive_size, vnode_t *dest_vn)
{
    if (!archive || !dest_vn)
        return EINVAL;

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

        // build full path
        char full_path[256];
        if (header->prefix[0] != '\0')
            snprintf(full_path, sizeof(full_path), "%s%s", header->prefix, header->name);
        else
            strncpy(full_path, header->name, sizeof(full_path) - 1);

        switch (header->typeflag)
        {
            case USTAR_DIRECTORY:
            {
                create_path(dest_vn, full_path, 1);
                break;
            }

            case USTAR_REGULAR:
            {
                vnode_t *file_vn = create_path(dest_vn, full_path, 0);
                if(file_vn && file_size > 0)
                {
                    uint64_t written;
                    if (vfs_write(file_vn, (void *)(data + offset), 0, file_size, &written) != EOK
                    ||  written != file_size)
                        log(LOG_ERROR, "USTAR: failed to write to created file");
                }
                break;
            }

            default:
                break;
        }

        uint64_t blocks = (file_size + USTAR_BLOCK_SIZE - 1) / USTAR_BLOCK_SIZE;
        offset += blocks * USTAR_BLOCK_SIZE;
    }

    log(LOG_INFO, "USTAR: loaded archive into filesystem");
    return EOK;
}
