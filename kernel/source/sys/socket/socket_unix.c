#include "mm/vm.h"
#include "sys/socket.h"

#include "sync/spinlock.h"
#include "sys/file.h"
#include "sys/socket.h"
#include "mm/mm.h"
#include "mm/heap.h"
#include "utils/ref.h"
#include "utils/string.h"
#include "utils/list.h"
#include "uapi/errno.h"

#define UNIX_PATH_MAX       256
#define UNIX_BACKLOG_MAX    32

struct sockaddr_un
{
    sa_family_t sun_family;
    char sun_path[UNIX_PATH_MAX];
};

typedef struct socket_unix socket_unix_t;

struct socket_unix
{
    socket_t so;

    int type;
    int state;
    socket_unix_t *peer;

    struct sockaddr_un *addr;

    list_t pending;

    void *buffer;
    size_t buffer_len;

    list_node_t table_node; // membership in unix_table
    list_node_t pending_node; // membership in server->pending

    spinlock_t lock;
    ref_t refcount;
};

/*
 * Socket states
 */

enum
{
    UNIX_STATE_INIT,
    UNIX_STATE_BOUND,
    UNIX_STATE_LISTEN,
    UNIX_STATE_CONNECTED,
    UNIX_STATE_CLOSED
};

/*
 * Global socket table
 */

typedef struct
{
    spinlock_t slock;
    list_t sockets;
}
unix_table_t;

static unix_table_t unix_table = { 0 };

/* Socket table operations */

static int unix_table_register(socket_unix_t *so)
{
    spinlock_acquire(&unix_table.slock);

    /* Reject duplicate paths */
    list_node_t *node = unix_table.sockets.head;
    while (node)
    {
        socket_unix_t *entry = LIST_GET_CONTAINER(node, socket_unix_t, table_node);
        if (memcmp(entry->addr, so->addr, sizeof(struct sockaddr_un)) == 0)
        {
            spinlock_release(&unix_table.slock);
            return EADDRINUSE;
        }
        node = node->next;
    }

    list_append(&unix_table.sockets, &so->table_node);

    spinlock_release(&unix_table.slock);
    return EOK;
}

static socket_unix_t *unix_table_lookup(const struct sockaddr_un *addr)
{
    spinlock_acquire(&unix_table.slock);

    socket_unix_t *found = NULL;
    list_node_t *node = unix_table.sockets.head;
    while (node)
    {
        socket_unix_t *entry = LIST_GET_CONTAINER(node, socket_unix_t, table_node);
        if (memcmp(entry->addr, addr, sizeof(struct sockaddr_un)) == 0)
        {
            found = entry;
            break;
        }
        node = node->next;
    }

    spinlock_release(&unix_table.slock);
    return found;
}

static void unix_table_unregister(socket_unix_t *so)
{
    spinlock_acquire(&unix_table.slock);
    list_remove(&unix_table.sockets, &so->table_node);
    spinlock_release(&unix_table.slock);
}

/*
 * Socket operations
 */

int unix_accept(socket_t *server, const struct sockaddr *addr, socklen_t addr_len, int flags, socket_t **out)
{
    socket_unix_t *server_unix = (socket_unix_t *)server;

    if (!server_unix->addr) // not bound
        return EINVAL;

    if (server_unix->state != UNIX_STATE_LISTEN) // not listening
        return EINVAL;

    if (list_is_empty(&server_unix->pending))
        return EAGAIN;

    // TO-DO: check nonblocking
    *out = (socket_t *)LIST_GET_CONTAINER(list_pop_head(&server_unix->pending), socket_unix_t, pending_node);

    return EOK;
}

int unix_bind(socket_t *so, const struct sockaddr *addr)
{
    socket_unix_t *so_unix = (socket_unix_t *)so;
    struct sockaddr_un *addr_un = (struct sockaddr_un *)addr;

    if (so_unix->state != UNIX_STATE_INIT)
        return EINVAL;

    so_unix->addr = heap_alloc(sizeof(struct sockaddr_un));
    memcpy(so_unix->addr, addr, sizeof(struct sockaddr_un));

    so_unix->state = UNIX_STATE_BOUND;

    int err = unix_table_register(so_unix);
    if (err != EOK)
        return err;

    return EOK;
}

int unix_connect(socket_t *client, const struct sockaddr *addr)
{
    socket_unix_t *client_unix = (socket_unix_t *)client;
    const struct sockaddr_un *addr_un = (const struct sockaddr_un*)addr;
    socket_unix_t *server;

    server = unix_table_lookup(addr_un);
    if (!server)
        return ENOENT;

    if (server->state != UNIX_STATE_LISTEN)
        return ECONNREFUSED;

    if (server->pending.length >= UNIX_BACKLOG_MAX)
        return ECONNREFUSED;

    list_append(&server->pending, &client_unix->pending_node);

    client_unix->peer = server;
    client_unix->state = UNIX_STATE_CONNECTED;

    return EOK;
}

int unix_listen(socket_t *so, int backlog)
{
    socket_unix_t *so_unix = (socket_unix_t *)so;
    if (so_unix->state != UNIX_STATE_BOUND)
        return EINVAL;

    if (backlog > UNIX_BACKLOG_MAX)
        backlog = UNIX_BACKLOG_MAX;

    so_unix->state = UNIX_STATE_LISTEN;

    return EOK;
}

ssize_t unix_recv(socket_t *so, void *buf, size_t len, int flags)
{
    socket_unix_t *so_unix = (socket_unix_t *)so;
    spinlock_acquire(&so_unix->lock);

    if (so_unix->buffer_len == 0)
    {
        spinlock_release(&so_unix->lock);
        return EAGAIN;
    }

    if (len > so_unix->buffer_len)
        len = so_unix->buffer_len;

    memcpy(buf, so_unix->buffer, len);

    memmove(
        so_unix->buffer,
        so_unix->buffer + len,
        so_unix->buffer_len - len
    );

    so_unix->buffer_len -= len;

    spinlock_release(&so_unix->lock);
    return len;
}

ssize_t unix_send(socket_t *so, const void *buf, size_t len, int flags)
{
    socket_unix_t *so_unix = (socket_unix_t *)so;
    if (!so_unix->peer)
        return ENOTCONN;

    socket_unix_t *peer = so_unix->peer;

    spinlock_acquire(&peer->lock);

    size_t space = sizeof(peer->buffer) - peer->buffer_len;
    if (len > space)
        len = space;

    memcpy(peer->buffer + peer->buffer_len, buf, len);
    peer->buffer_len += len;

    spinlock_release(&peer->lock);
    return len;
}

struct socket_ops unix_ops = {
    .accept      = unix_accept,
    .bind        = unix_bind,
    .connect     = unix_connect,
    .listen      = unix_listen,
    .recv        = unix_recv,
    .send        = unix_send
};

/* Domain ops */
int socket_create_unix(int type, [[maybe_unused]] int protocol, socket_t **so)
{
    if (type != SOCK_STREAM)
        return EPROTONOSUPPORT;

    socket_unix_t *u = heap_alloc(sizeof(socket_unix_t));
    if (!u)
        return ENOMEM;

    u->so.domain = AF_UNIX;
    u->so.ops = &unix_ops;

    u->type  = type;
    u->state = UNIX_STATE_INIT;
    u->peer = NULL;
    u->pending = LIST_INIT;

    u->buffer = vm_alloc(4096);
    if (!u->buffer)
    {
        heap_free(u);
        return ENOMEM;
    }
    u->buffer_len = 4096;

    u->lock = SPINLOCK_INIT;

    return EOK;
}

int socket_destroy_unix(socket_t *so)
{
    if (!so) return EINVAL;

    socket_unix_t *u = (socket_unix_t *)so;
    if (u->state == UNIX_STATE_BOUND || u->state == UNIX_STATE_LISTEN)
        unix_table_unregister(u);

    u->peer = NULL;
    u->state = UNIX_STATE_CLOSED;

    heap_free(u);
    heap_free(so);

    return EOK;
}
