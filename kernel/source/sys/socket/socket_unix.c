#include "sys/socket.h"

#include "sync/spinlock.h"
#include "sys/file.h"
#include "sys/socket.h"
#include "mm/mm.h"
#include "utils/ref.h"
#include "utils/string.h"
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

    socket_unix_t *pending[UNIX_BACKLOG_MAX];
    int pending_count;

    /* simple receive buffer */
    char buffer[4096];
    size_t buffer_len;

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
 * Socket operations
 */

int unix_accept(socket_unix_t *server, socket_unix_t **out)
{
    if (server->state != UNIX_STATE_LISTEN)
        return -EINVAL;

    if (server->pending_count == 0)
        return -EAGAIN;

    socket_unix_t *client = server->pending[0];

    memmove(
        &server->pending[0],
        &server->pending[1],
        sizeof(server->pending[0]) * (server->pending_count - 1)
    );

    server->pending_count--;

    *out = client;

    return 0;
}

int unix_bind(socket_unix_t *so, const struct sockaddr_un *addr)
{
    if (so->state != UNIX_STATE_INIT)
        return -EINVAL;

    strncpy(so->path, addr->sun_path, UNIX_PATH_MAX);

    so->state = UNIX_STATE_BOUND;

    /* TODO: register in global unix socket table */

    return 0;
}

int unix_connect(socket_unix_t *client, const struct sockaddr_un *addr)
{
    socket_unix_t *server;

    /* lookup socket by path */
    server = unix_lookup(addr->sun_path);
    if (!server)
        return -ENOENT;

    if (server->state != UNIX_STATE_LISTEN)
        return -ECONNREFUSED;

    if (server->pending_count >= UNIX_BACKLOG_MAX)
        return -ECONNREFUSED;

    server->pending[server->pending_count++] = client;

    client->peer = server;
    client->state = UNIX_STATE_CONNECTED;

    return 0;
}

int unix_listen(socket_unix_t *so, int backlog)
{
    if (so->state != UNIX_STATE_BOUND)
        return -EINVAL;

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
        return -EAGAIN;
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
        return -ENOTCONN;

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
