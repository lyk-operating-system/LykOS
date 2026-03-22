#include "mm/vm.h"
#include "sys/socket.h"

#include "log.h"
#include "mm/heap.h"
#include "mm/mm.h"
#include "sync/spinlock.h"
#include "sys/file.h"
#include "sys/sched.h"
#include "sys/socket.h"
#include "uapi/errno.h"
#include "utils/container_of.h"
#include "utils/list.h"
#include "utils/ref.h"
#include "utils/string.h"
#include <stdint.h>

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

    // Ring buffer
    void *buffer;
    size_t capacity;
    size_t head;
    size_t length;

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
    UNIX_STATE_CONNECTING,
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
        if (strncmp(entry->addr->sun_path, so->addr->sun_path, UNIX_PATH_MAX) == 0)
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
    FOREACH(node, unix_table.sockets)
    {
        socket_unix_t *entry = LIST_GET_CONTAINER(node, socket_unix_t, table_node);
        if (strncmp(entry->addr->sun_path, addr->sun_path, UNIX_PATH_MAX) == 0)
        {
            found = entry;
            break;
        }
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

    bool nonblock = (flags & SOCK_NONBLOCK) || (server->flags & SOCK_NONBLOCK);
    while (list_is_empty(&server_unix->pending))
    {
        if (nonblock)
            return EAGAIN;

        sched_yield(THREAD_STATE_READY);
    }

    socket_t *new;
    int err = socket_create(AF_UNIX, SOCK_STREAM, 0, &new);
    if (err != EOK)
        return err;

    socket_unix_t *peer = container_of(
        list_pop_head(&server_unix->pending),
        socket_unix_t,
        pending_node
    );
    peer->state = UNIX_STATE_CONNECTED;
    peer->peer = (socket_unix_t *)new;

    ((socket_unix_t *)new)->peer = peer;
    ((socket_unix_t *)new)->state = UNIX_STATE_CONNECTED;


    *out = new;
    return EOK;
}

int unix_bind(socket_t *so, const struct sockaddr *addr)
{
    socket_unix_t *so_unix = (socket_unix_t *)so;

    if (so_unix->state != UNIX_STATE_INIT)
        return EINVAL;

    so_unix->addr = heap_alloc(sizeof(struct sockaddr_un));
    memcpy(so_unix->addr, addr, sizeof(struct sockaddr_un));

    so_unix->state = UNIX_STATE_BOUND;

    int err = unix_table_register(so_unix);
    if (err != EOK)
    {
        heap_free(so_unix->addr);
        so_unix->addr = NULL;
        return err;
    }

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

    if (server->state != UNIX_STATE_LISTEN
    ||  server->pending.length >= UNIX_BACKLOG_MAX)
        return ECONNREFUSED;

    client_unix->state = UNIX_STATE_CONNECTING;
    client_unix->peer = NULL;

    list_append(&server->pending, &client_unix->pending_node);

    while (client_unix->state == UNIX_STATE_CONNECTING)
        sched_yield(THREAD_STATE_READY);

    return (client_unix->state == UNIX_STATE_CONNECTED) ? EOK : ECONNREFUSED;
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

int unix_recv(socket_t *so, void *buf, size_t len, int flags,
              thread_t *t, uint64_t *recv_bytes)
{
    socket_unix_t *u = (socket_unix_t *)so;

    spinlock_acquire(&u->lock);

    if (u->length == 0)
    {
        spinlock_release(&u->lock);
        return EAGAIN;
    }

    if (len > u->length)
        len = u->length;

    size_t first = u->capacity - u->head;
    if (first > len)
        first = len;

    vm_addrspace_t *as = t->owner->as;

    // first chunk
    vm_copy_to_user(as,
        (uintptr_t)buf,
        (void *)((uintptr_t)u->buffer + u->head),
        first
    );

    // wrap-around chunk
    if (len > first)
    {
        vm_copy_to_user(as,
            (uintptr_t)buf + first,
            u->buffer,
            len - first
        );
    }

    u->head = (u->head + len) % u->capacity;
    u->length -= len;

    *recv_bytes = len;

    spinlock_release(&u->lock);
    return EOK;
}

int unix_send(socket_t *so, const void *buf, size_t len, int flags,
              thread_t *t, uint64_t *sent_bytes)
{
    socket_unix_t *u = (socket_unix_t *)so;
    if (!u->peer)
        return ENOTCONN;

    socket_unix_t *peer = u->peer;

    spinlock_acquire(&peer->lock);

    size_t space = peer->capacity - peer->length;
    if (space == 0)
    {
        spinlock_release(&peer->lock);
        return EAGAIN;
    }

    if (len > space)
        len = space;

    size_t tail = (peer->head + peer->length) % peer->capacity;

    size_t first = peer->capacity - tail;
    if (first > len)
        first = len;

    vm_addrspace_t *as = t->owner->as;

    // first chunk
    vm_copy_from_user(as,
        (void *)((uintptr_t)peer->buffer + tail),
        (uintptr_t)buf,
        first
    );

    // wrap-around chunk
    if (len > first)
    {
        vm_copy_from_user(as,
            peer->buffer,
            (uintptr_t)buf + first,
            len - first
        );
    }

    peer->length += len;
    *sent_bytes = len;

    spinlock_release(&peer->lock);
    return EOK;
}

struct socket_ops unix_ops = {
    .accept      = unix_accept,
    .bind        = unix_bind,
    .connect     = unix_connect,
    .listen      = unix_listen,
    .recv        = unix_recv,
    .send        = unix_send
};

/*
 * Domain ops
 */

int socket_create_unix(int type, [[maybe_unused]] int protocol, socket_t **so)
{
    if (type != SOCK_STREAM)
        return EPROTONOSUPPORT;

    socket_unix_t *u = heap_alloc(sizeof(socket_unix_t));
    if (!u)
        return ENOMEM;

    void *buffer = vm_alloc(4096);
    if (!buffer)
    {
        heap_free(u);
        return ENOMEM;
    }

    *u = (socket_unix_t) {
        .so = {
            .domain = AF_UNIX,
            .ops = &unix_ops,
        },
        .type     = type,
        .state    = UNIX_STATE_INIT,
        .peer     = NULL,
        .addr     = NULL,
        .pending  = LIST_INIT,

        .buffer   = buffer,
        .capacity = 4096,
        .head     = 0,
        .length   = 0,

        .lock     = SPINLOCK_INIT,
        .refcount = 1
    };

    *so = (socket_t *)u;
    return EOK;
}

int socket_destroy_unix(socket_t *so)
{
    socket_unix_t *u = (socket_unix_t *)so;

    // unregister if it was ever bound
    if (u->addr)
    {
        unix_table_unregister(u);
        heap_free(u->addr);
        u->addr = NULL;
    }

    if (u->buffer)
    {
        vm_free(u->buffer);
        u->buffer = NULL;
    }

    // break peer linkage
    if (u->peer)
    {
        spinlock_acquire(&u->peer->lock);
        if (u->peer->peer == u)
            u->peer->peer = NULL;
        spinlock_release(&u->peer->lock);

        u->peer = NULL;
    }

    u->state = UNIX_STATE_CLOSED;

    heap_free(u);
    return EOK;
}
