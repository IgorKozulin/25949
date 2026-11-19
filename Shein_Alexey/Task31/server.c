#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CLIENTS 32

static volatile sig_atomic_t stopping = 0;

static void stop_server(int sig)
{
    (void)sig;
    stopping = 1;
}

static int make_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);

    if (flags == -1) {
        return -1;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void disconnect_client(struct pollfd *entry)
{
    fprintf(stderr, "[SERVER] Client fd=%d disconnected\n",
            entry->fd);
    close(entry->fd);
    entry->fd = -1;
    entry->revents = 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_un address;
    struct sigaction action;
    struct pollfd fds[MAX_CLIENTS + 1];
    int server_fd = -1;
    int bound = 0;
    int status = EXIT_FAILURE;
    int i;

    for (i = 0; i <= MAX_CLIENTS; ++i) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s socket_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strlen(argv[1]) >= sizeof(address.sun_path)) {
        fprintf(stderr, "Socket path is too long\n");
        return EXIT_FAILURE;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = stop_server;

    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, argv[1]);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        goto cleanup;
    }

    fds[0].fd = server_fd;

    if (make_nonblocking(server_fd) == -1) {
        perror("fcntl server");
        goto cleanup;
    }

    if (bind(server_fd, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("bind");
        goto cleanup;
    }
    bound = 1;

    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        goto cleanup;
    }

    fprintf(stderr, "[SERVER] Waiting on %s\n", argv[1]);
    fprintf(stderr, "[SERVER] Ctrl+C to stop. Maximum clients: %d\n",
            MAX_CLIENTS);

    while (!stopping) {
        int ready = poll(fds, MAX_CLIENTS + 1, 1000);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            goto cleanup;
        }

        if (stopping) {
            break;
        }

        if (ready == 0) {
            continue;
        }

        for (i = 1; i <= MAX_CLIENTS; ++i) {
            unsigned char buffer[4096];
            ssize_t n;
            ssize_t j;
            short events = fds[i].revents;

            if (fds[i].fd == -1 || events == 0) {
                continue;
            }

            if (events & POLLNVAL) {
                disconnect_client(&fds[i]);
                continue;
            }

            if (!(events & (POLLIN | POLLHUP | POLLERR))) {
                continue;
            }

            n = read(fds[i].fd, buffer, sizeof(buffer));

            if (n > 0) {
                for (j = 0; j < n; ++j) {
                    buffer[j] = (unsigned char)toupper(buffer[j]);
                }

                if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n ||
                    fflush(stdout) == EOF) {
                    perror("stdout");
                    goto cleanup;
                }
            } else if (n == 0) {
                disconnect_client(&fds[i]);
            } else {
                if (errno == EINTR) {
                    continue;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (events & (POLLHUP | POLLERR)) {
                        disconnect_client(&fds[i]);
                    }
                    continue;
                }

                perror("read client");
                disconnect_client(&fds[i]);
            }
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "[SERVER] Listening socket failed\n");
            goto cleanup;
        }

        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);
            int slot;

            if (client_fd == -1) {
                if (errno == EINTR ||
                    errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                perror("accept");
                goto cleanup;
            }

            for (slot = 1; slot <= MAX_CLIENTS; ++slot) {
                if (fds[slot].fd == -1) {
                    break;
                }
            }

            if (slot > MAX_CLIENTS) {
                fprintf(stderr, "[SERVER] Too many clients\n");
                close(client_fd);
                continue;
            }

            if (make_nonblocking(client_fd) == -1) {
                perror("fcntl client");
                close(client_fd);
                continue;
            }

            fds[slot].fd = client_fd;
            fds[slot].events = POLLIN;
            fds[slot].revents = 0;

            fprintf(stderr, "[SERVER] Client connected, fd=%d\n",
                    client_fd);
        }
    }

    status = EXIT_SUCCESS;
    fprintf(stderr, "\n[SERVER] Stopping\n");

cleanup:
    for (i = 0; i <= MAX_CLIENTS; ++i) {
        if (fds[i].fd != -1 && close(fds[i].fd) == -1) {
            perror("close");
            status = EXIT_FAILURE;
        }
    }

    if (bound && unlink(argv[1]) == -1) {
        perror("unlink");
        status = EXIT_FAILURE;
    }

    return status;
}
