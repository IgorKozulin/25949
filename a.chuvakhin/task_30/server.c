#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "./socket"

int main(void)
{
    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    int client = -1, result = 1;
    struct sockaddr_un address = {.sun_family = AF_UNIX, .sun_path = SOCKET_PATH};
    char buffer[1024];

    if (server == -1) {
        perror("socket");
        return 1;
    }

    unlink(SOCKET_PATH);

    if (bind(server, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server);
        return 1;
    }
    if (listen(server, 1) == -1)
        goto cleanup;

    client = accept(server, NULL, NULL);
    
    if (client == -1)
        goto cleanup;

    for (;;) {
        ssize_t count = read(client, buffer, sizeof(buffer));
        if (count == 0)
            break;
        if (count == -1) {
            if (errno == EINTR)
                continue;
            goto cleanup;
        }
        for (ssize_t i = 0; i < count; ++i)
            buffer[i] = (char)toupper((unsigned char)buffer[i]);
        if (fwrite(buffer, 1, (size_t)count, stdout) != (size_t)count ||
            fflush(stdout) == EOF)
            goto cleanup;
    }
    result = 0;

cleanup:
    if (result != 0)
        perror("server");
    if (client != -1)
        close(client);
    close(server);
    unlink(SOCKET_PATH);
    return result;
}
