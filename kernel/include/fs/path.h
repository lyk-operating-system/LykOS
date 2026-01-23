#pragma once

#include <stddef.h>

#define PATH_MAX 512

/**
 * @brief
 */
bool path_is_absolute(const char* path);

/**
 * @brief Canonicalize a path.
 *
 * Collapses multiple slashes and resolves "." and "..".
 *
 * @param path Mutable path buffer.
 */
bool path_canonicalize(const char *path, char *out);

/**
 * @brief Split path into parent path and final component name.
 */
void path_split(const char *path,
                char *restrict out_dirname, size_t *out_dirname_len,
                char *restrict out_basename, size_t *out_basename_len);

/**
 * @brief Returns the basename (last component).
 */
void path_basename(const char* path, char *out, size_t *out_len);

/**
 * @brief Returns the canonicalized dirname (the path without the last component).
 */
void path_dirname(const char* path, char *out, size_t *out_len);

/**
 * @brief Join two paths into one.
 *
 * @note If `b` is absolute, it replaces a.
 */
bool path_join(const char *a, const char *b, char *out, size_t *out_len);

/**
 * @brief Fetch the next component in a path for iterative walking.
 *
 * @return Pointer to the next character following the extracted component.
 */
const char *path_next_component(const char *path, char *out, size_t *out_len);
