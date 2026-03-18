#pragma once

#include "sys/types.h"

/*
 * Forward declarations
 */

typedef struct socket socket_t;
typedef struct socket_ops socket_ops_t;
typedef struct socket_domain socket_domain_t;

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

/*
 * LykOS impl-specific
 */

struct socket
{
    int domain;
    socket_ops_t *ops;
};

struct socket_ops
{
    int     (*accept)(socket_t *server, const struct sockaddr *addr, socklen_t addr_len, int flags, socket_t **out);

    int     (*bind)(socket_t *so, const struct sockaddr *addr);
    int     (*connect)(socket_t *client, const struct sockaddr *addr);
    int     (*listen)(socket_t *so, int backlog);

    int     (*getsockname)(socket_t *so, struct sockaddr *addr);
    int     (*getpeername)(socket_t *so, struct sockaddr *addr);

    ssize_t (*recv)(socket_t *so, void *buf, size_t len, int flags);
    ssize_t (*send)(socket_t *so, const void *buf, size_t len, int flags);

    int     (*shutdown)(socket_t *so, int how);
};

int socket_create(int domain, int type, int protocol, socket_t **so);

int socket_destroy(socket_t *so);
