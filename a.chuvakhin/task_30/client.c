#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "./socket"

int main(void)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    int result = 1;
    struct sockaddr_un address = {.sun_family = AF_UNIX, .sun_path = SOCKET_PATH};
    char buffer[1024];

    if (sock == -1) {
        perror("socket");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) == -1)
        goto cleanup;

    for (;;) {
        ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count == 0)
            break;
        if (count == -1) {
            if (errno == EINTR)
                continue;
            goto cleanup;
        }
        for (ssize_t sent = 0; sent < count;) {
            ssize_t n = write(sock, buffer + sent, (size_t)(count - sent));
            if (n == -1 && errno == EINTR)
                continue;
            if (n <= 0)
                goto cleanup;
            sent += n;
        }
    }
    result = 0;

cleanup:
    if (result != 0)
        perror("client");
    close(sock);
    return result;
}
