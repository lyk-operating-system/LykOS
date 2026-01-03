#pragma once

#include <stddef.h>
#include <stdint.h>
#include "fs/vfs.h"

#define USTAR_REGULAR   '0'
#define USTAR_HARDLINK  '1'
#define USTAR_SYMLINK   '2'
#define USTAR_CHAR      '3'
#define USTAR_BLOCK     '4'
#define USTAR_DIRECTORY '5'
#define USTAR_FIFO      '6'
#define USTAR_CONTIG    '7'

typedef struct
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;        // '0' or '\0' = file, '5' = dir
    char linkname[100];
    char magic[6];        // "ustar\0"
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} __attribute__((packed))
ustar_header_t;


int ustar_extract(const void *archive, uint64_t archive_size, const char *dest_path);
