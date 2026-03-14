#include "sys/socket.h"

#include "sync/spinlock.h"
#include "sys/file.h"
#include "sys/socket.h"
#include "mm/mm.h"
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
    int type;
    int state;
    socket_unix_t *peer;

    char path[UNIX_PATH_MAX];

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
        if (strncmp(entry->path, so->path, UNIX_PATH_MAX) == 0)
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

static socket_unix_t *unix_table_lookup(const char *path)
{
    spinlock_acquire(&unix_table.slock);

    socket_unix_t *found = NULL;
    list_node_t *node = unix_table.sockets.head;
    while (node)
    {
        socket_unix_t *entry = LIST_GET_CONTAINER(node, socket_unix_t, table_node);
        if (strncmp(entry->path, path, UNIX_PATH_MAX) == 0)
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

int unix_accept(socket_unix_t *server, socket_unix_t **out)
{
    if (server->state != UNIX_STATE_LISTEN)
        return EINVAL;

    if (list_is_empty(&server->pending))
        return EAGAIN;


    *out = LIST_GET_CONTAINER(list_pop_head(&server->pending), socket_unix_t, pending_node);

    return EOK;
}

int unix_bind(socket_unix_t *so, const struct sockaddr_un *addr)
{
    if (so->state != UNIX_STATE_INIT)
        return EINVAL;

    strncpy(so->path, addr->sun_path, UNIX_PATH_MAX);

    so->state = UNIX_STATE_BOUND;

    /* TODO: register in global unix socket table */

    return 0;
}

int unix_connect(socket_unix_t *client, const struct sockaddr_un *addr)
{
    socket_unix_t *server;

    server =
    if (!server)
        return ENOENT;

    if (server->state != UNIX_STATE_LISTEN)
        return ECONNREFUSED;

    if (server->pending.length >= UNIX_BACKLOG_MAX)
        return ECONNREFUSED;

    list_append(&server->pending, &client->pending_node);

    client->peer = server;
    client->state = UNIX_STATE_CONNECTED;

    return 0;
}

int unix_listen(socket_unix_t *so, int backlog)
{
    if (so->state != UNIX_STATE_BOUND)
        return EINVAL;

    if (backlog > UNIX_BACKLOG_MAX)
        backlog = UNIX_BACKLOG_MAX;

    so->state = UNIX_STATE_LISTEN;

    return 0;
}

ssize_t unix_recv(socket_unix_t *so, void *buf, size_t len)
{
    spinlock_acquire(&so->lock);

    if (so->buffer_len == 0)
    {
        spinlock_release(&so->lock);
        return EAGAIN;
    }

    if (len > so->buffer_len)
        len = so->buffer_len;

    memcpy(buf, so->buffer, len);

    memmove(
        so->buffer,
        so->buffer + len,
        so->buffer_len - len
    );

    so->buffer_len -= len;

    spinlock_release(&so->lock);
    return len;
}

ssize_t unix_send(socket_unix_t *so, const void *buf, size_t len)
{
    if (!so->peer)
        return ENOTCONN;

    socket_unix_t *peer = so->peer;

    spinlock_acquire(&peer->lock);

    size_t space = sizeof(peer->buffer) - peer->buffer_len;
    if (len > space)
        len = space;

    memcpy(peer->buffer + peer->buffer_len, buf, len);
    peer->buffer_len += len;

    spinlock_release(&peer->lock);
    return len;
}
