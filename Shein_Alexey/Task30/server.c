#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    struct sockaddr_un address;
    const char *path;
    int server_fd = -1;
    int client_fd = -1;
    int bound = 0;
    int status = EXIT_FAILURE;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s socket_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    path = argv[1];

    if (strlen(path) >= sizeof(address.sun_path)) {
        fprintf(stderr, "Socket path is too long\n");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        goto cleanup;
    }

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("bind");
        goto cleanup;
    }
    bound = 1;

    if (listen(server_fd, 1) == -1) {
        perror("listen");
        goto cleanup;
    }

    fprintf(stderr, "[SERVER] Waiting on %s\n", path);

    do {
        client_fd = accept(server_fd, NULL, NULL);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1) {
        perror("accept");
        goto cleanup;
    }

    fprintf(stderr, "[SERVER] Client connected\n");

    for (;;) {
        unsigned char buffer[4096];
        ssize_t n = read(client_fd, buffer, sizeof(buffer));
        ssize_t i;

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            goto cleanup;
        }

        if (n == 0) {
            break;
        }

        for (i = 0; i < n; ++i) {
            buffer[i] = (unsigned char)toupper(buffer[i]);
        }

        if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n ||
            fflush(stdout) == EOF) {
            perror("stdout");
            goto cleanup;
        }
    }

    fprintf(stderr, "[SERVER] Client disconnected. Exiting.\n");
    status = EXIT_SUCCESS;

cleanup:
    if (client_fd != -1 && close(client_fd) == -1) {
        perror("close client");
        status = EXIT_FAILURE;
    }

    if (server_fd != -1 && close(server_fd) == -1) {
        perror("close server");
        status = EXIT_FAILURE;
    }

    if (bound && unlink(path) == -1) {
        perror("unlink");
        status = EXIT_FAILURE;
    }

    return status;
}
