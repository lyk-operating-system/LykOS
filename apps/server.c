#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/af_unix_test.sock"

int main() {
    int sfd, cfd;
    struct sockaddr_un addr;
    char buf[128];

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) {
        puts("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        puts("bind");
        exit(1);
    }

    if (listen(sfd, 1) < 0) {
        puts("listen");
        exit(1);
    }

    cfd = accept(sfd, NULL, NULL);
    if (cfd < 0) {
        puts("accept");
        exit(1);
    }

    int n = read(cfd, buf, sizeof(buf));
    if (n > 0)
    {
        write(cfd, "ok", 2);     // reply
    }
    printf("server %d %s", n, buf);
    fflush(stdout);

    close(cfd);
    close(sfd);
    return 0;
}
