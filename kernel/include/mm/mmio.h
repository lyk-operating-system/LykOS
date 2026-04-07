#pragma once

#include <stddef.h>
#include <stdint.h>

void *mmio_map(uintptr_t phys, size_t size);
int mmio_unmap(void *virt, size_t size);
