#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 100
#define SOCKET_PATH "./socket"

static volatile sig_atomic_t running = 1;

static void stop(int signal_number)
{
    (void)signal_number;
    running = 0;
}

int main(void)
{
    struct sigaction action = { .sa_handler = stop };
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un address = {
        .sun_family = AF_UNIX,
        .sun_path = SOCKET_PATH
    };
    unlink(SOCKET_PATH);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    struct pollfd fds[MAX_CLIENTS + 1];
    nfds_t nfds = 1;
    int result = 1;

    fds[0] = (struct pollfd){ .fd = server_fd, .events = POLLIN };
    for (nfds_t i = 1; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1;
    }

    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        goto cleanup;
    }
    fprintf(stderr, "Server listening on %s. Ctrl+C to stop.\n", SOCKET_PATH);

    while (running) {
        if (poll(fds, nfds, 1000) == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            goto cleanup;
        }

        for (nfds_t i = 1; i < nfds; i++) {
            short events = fds[i].revents;
            if (events == 0) {
                continue;
            }

            if (events & (POLLIN | POLLHUP)) {
                unsigned char buffer[1024];
                ssize_t count = read(fds[i].fd, buffer, sizeof(buffer));

                if (count > 0) {
                    for (ssize_t j = 0; j < count; j++) {
                        buffer[j] = toupper(buffer[j]);
                    }
                    if (fwrite(buffer, 1, (size_t)count, stdout) != (size_t)count ||
                        fflush(stdout) == EOF) {
                        perror("stdout");
                        goto cleanup;
                    }
                    continue;
                }
            }

            close(fds[i].fd);
            fds[i].fd = -1;
        }

        while (nfds > 1 && fds[nfds - 1].fd == -1) {
            nfds--;
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fputs("Listening socket error\n", stderr);
            goto cleanup;
        }

        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd == -1) {
                if (errno != EINTR) {
                    perror("accept");
                }
                continue;
            }

            nfds_t slot = 1;
            while (slot <= MAX_CLIENTS && fds[slot].fd != -1) {
                slot++;
            }

            if (slot > MAX_CLIENTS) {
                close(client_fd);
                continue;
            }
            fds[slot] = (struct pollfd){ .fd = client_fd, .events = POLLIN };
            if (slot == nfds) {
                nfds++;
            }
        }
    }
    result = 0;

cleanup:
    for (nfds_t i = 0; i < nfds; i++) {
        if (fds[i].fd != -1) {
            close(fds[i].fd);
        }
    }
    unlink(SOCKET_PATH);
    return result;
}
