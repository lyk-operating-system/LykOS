#pragma once

#include <stddef.h>

size_t djb2(const char *str);
size_t djb2_len(const char *str, size_t len);
