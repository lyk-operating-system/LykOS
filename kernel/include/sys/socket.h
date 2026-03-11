#pragma once

#include "sys/types.h"

/*
 * Socket types
 */

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3
#define SOCK_RDM        4
#define SOCK_SEQPACKET  5

/*
 * Socket flags
 */

#define SOCK_NONBLOCK   0x800
#define SOCK_CLOEXEC    0x80000

/*
 * Address families
 */

#define AF_UNSPEC       0
#define AF_UNIX         1
#define AF_LOCAL        AF_UNIX

/*
 * Protocol families (same values as AF_*)
 */

#define PF_UNSPEC       AF_UNSPEC
#define PF_UNIX         AF_UNIX
#define PF_INET         AF_INET
#define PF_INET6        AF_INET6
#define PF_NETLINK      AF_NETLINK

/*
 * Socket shutdown
 */

#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

/*
 * Message flags (send/recv)
 */

#define MSG_OOB         0x1
#define MSG_PEEK        0x2
#define MSG_DONTWAIT    0x40
#define MSG_WAITALL     0x100
#define MSG_NOSIGNAL    0x4000

/*
 * Socket address base structure
 */

typedef uint16_t sa_family_t;
typedef uint32_t socklen_t;

struct sockaddr
{
    sa_family_t sa_family;
    char sa_data[14];
};


#define SOCKET_MAXADDRLEN   255
