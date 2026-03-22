#include "sys/socket.h"
#include "uapi/errno.h"

extern int socket_create_unix(int type, int protocol, socket_t **so);
int socket_create(int domain, int type, int protocol, socket_t **so)
{
    switch (domain)
    {
        case AF_UNIX:
            return socket_create_unix(type, protocol, so);

        default:
            return EAFNOSUPPORT;
    }
}

extern int socket_destroy_unix(socket_t *so);
int socket_destroy(socket_t *so)
{
    switch (so->domain)
    {
        case AF_UNIX:
            return socket_destroy_unix(so);

        default:
            return EAFNOSUPPORT;
    }
}
