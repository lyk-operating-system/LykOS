#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/af_unix_test.sock"

int main() {
    int fd;
    struct sockaddr_un addr;
    char buf[128];

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        puts("socket");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        puts("connect");
        exit(1);
    }

    write(fd, "hello", 5);

    close(fd);
    return 0;
}
