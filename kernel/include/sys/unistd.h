#pragma once

/*
 * Access mode
 */
#define O_RDONLY    0x00000
#define O_WRONLY    0x00001
#define O_RDWR      0x00002
#define O_EXEC      0x00003
#define O_SEARCH    0x00004

#define O_ACCMODE   0x00007

/*
 * Creation
 */
#define O_CREAT     0x00008
#define O_EXCL      0x00010
#define O_TRUNC     0x00020
#define O_DIRECTORY 0x00040
#define O_NOFOLLOW  0x00080
#define O_NOCTTY    0x00100
#define O_TTY_INIT  0x00200

/*
 * FD behavior
 */
#define O_CLOEXEC   0x00400
#define O_CLOFORK   0x00800

/*
 * IO behavior
 */
#define O_APPEND    0x01000
#define O_NONBLOCK  0x02000
#define O_SYNC      0x04000
#define O_DSYNC     0x08000
#define O_RSYNC     0x10000

/*
 * Seek
 */
#define SEEK_SET    0x0
#define SEEK_CUR    0x1
#define SEEK_END    0x2
#define SEEK_HOLE   0x4
#define SEEK_DATA   0x8
