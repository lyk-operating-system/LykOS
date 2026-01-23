#include "fs/path.h"

#include "assert.h"
#include "mm/mm.h"
#include "utils/string.h"

bool path_is_absolute(const char* path)
{
    return path[0] == '/' ? true : false;
}

bool path_canonicalize(const char *path, char *out)
{
    ASSERT(path && out);

    const char *src = path;
    char *dest = out;

    // Skip initial '/' (root)
    if (*path == '/')
        *dest++ = *src++;

    while (*src)
    {
        // Skip consecutive slashes
        if (*src == '/' && *(dest - 1) == '/')
        {
            src++;
            continue;
        }

        // Handle '.'
        if (src[0] == '.' && (src[1] == '/' || src[1] == '\0'))
        {
            src += (src[1] == '/') ? 2 : 1;
            continue;
        }

        // Handle '..'
        if (src[0] == '.'&& src[1] == '.' && (src[2] == '/' || src[2] == '\0'))
        {
            if (dest > path + 1)
            {
                dest--; // Skip preceding '/'
                while (dest > path && *(dest - 1) != '/')
                    dest--;
            }

            src += (src[2] == '/') ? 3 : 2;
            continue;
        }

        // Normal character
        *dest++ = *src++;
    }

    /*
     * Remove trailing slash except for root ('/').
     * example: /foo/bar/ becomes /foo/bar
     */
    if (dest > path + 1 && *(dest - 1) == '/')
        dest--;

    *dest = '\0';
    return true;
}

void path_split(const char *path,
                char *restrict dirname, size_t *out_dirname_len,
                char *restrict basename, size_t *out_basename_len)
{
    ASSERT(path && out_dirname_len && out_basename_len);

    const char *slash = strrchr(path, '/');
    if (slash)
    {
        size_t dlen = slash - path;
        if (dirname)
        {
            if (dlen == 0)
            {
                dirname[0] = '/';
                dirname[1] = '\0';
            }
            else
            {
                memcpy(dirname, path, dlen);
                dirname[dlen] = '\0';
            }
        }
        *out_dirname_len = dlen == 0 ? 1 : dlen;

        if (basename)
            strcpy(basename, slash + 1);
        *out_basename_len = strlen(slash + 1);
    }
    else
    {
        if (dirname)
            strcpy(dirname, ".");
        *out_dirname_len = 1;

        if (basename)
            strcpy(basename, path);
        *out_basename_len = strlen(path);
    }
}

void path_basename(const char* path, char *out, size_t *out_len)
{
    ASSERT(path && out_len);

    const char *slash = strrchr(path, '/');
    const char *basename = slash ? slash + 1 : path;
    if (out)
        strcpy(out, basename);
    *out_len = strlen(basename);
}

void path_dirname(const char* path, char *out, size_t *out_len)
{
    ASSERT(path && out_len);

    const char *slash = strrchr(path, '/');
    if (slash)
    {
        size_t dlen = slash - path;
        if (out)
        {
            if (dlen == 0)
            {
                out[0] = '/';
                out[1] = '\0';
            }
            else
            {
                memcpy(out, path, dlen);
                out[dlen] = '\0';
            }
        }
        *out_len = dlen == 0 ? 1 : dlen;
    }
    else
    {
        if (out)
            strcpy(out, ".");
        *out_len = 1;
    }
}

bool path_join(const char *a, const char *b, char *out, size_t *out_len)
{
    ASSERT(a && b && out_len);

    if (path_is_absolute(b))
    {
        if (out)
            strcpy(out, b);
        *out_len = strlen(b);
        return true;
    }

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    bool need_slash = (len_a > 0 && a[len_a - 1] != '/');

    if (out)
    {
        if (len_a)
            strcpy(out, a);
        if (need_slash)
            strcat(out, "/");
        strcat(out, b);
    }

    *out_len = len_a + (need_slash ? 1 : 0) + len_b;
    return true;
}

void path_next_component(const char *path, char *out, size_t *out_len)
{
    while (*path == '/')
        path++;

    const char *start = path;
    const char *end = start;
    while (*end && *end != '/')
        end++;

    *out_len = end - start;
    if (out)
    {
        memcpy(out, start, *out_len);
        out[*out_len] = '\0';
    }
}
