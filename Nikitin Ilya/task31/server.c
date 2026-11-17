#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <string.h>

#define SOCKET_PATH "./socket"
#define MAX_CLIENTS 32
#define BUFFER_SIZE 1024

// флаг завершения сервера
static volatile sig_atomic_t stop_server = 0;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    stop_server = 1;
}

static void disconnect_client(struct pollfd fds[], nfds_t *nfds, nfds_t index, int *clients_count)
{
    int client_fd = fds[index].fd;
    if (client_fd = -1)
    {
        return;
    }

    printf("[SERVER] Client %d disconect\n", client_fd);
    close(client_fd);

    fds[index].fd = -1;
    fds[index].events = 0;
    fds[index].revents = 0;

    if (*clients_count > 0)
    {
        (*clients_count)--;
    }

    while (*nfds > 1 && fds[*nfds - 1].fd == -1)
    {
        (*nfds)--;
    }

    printf("[SERVER] clients remaind: %d\n", *clients_count);
}

int main(void)
{
    int server_fd;
    int clients_count = 0;
    int exit_code = EXIT_SUCCESS;

    nfds_t nfds;
    nfds_t i;

    struct sockaddr_un server_addr;
    struct pollfd fds[MAX_CLIENTS + 1];
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    if (unlink(SOCKET_PATH) == -1 && errno != ENOENT)
    {
        perror("unlink");
        return EXIT_FAILURE;
    }

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&server_fd, 0, sizeof(server_fd));
    server_addr.sun_family = AF_UNIX;

    if (strlen(SOCKET_PATH) >= sizeof(server_addr.sun_path))
    {
        fprintf(stderr, "Too lonh way to socket\n");
        close(server_fd);
        return EXIT_FAILURE;
    }

    strcpy(server_addr.sun_path, SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, MAX_CLIENTS) == -1)
    {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    for (i = 0; i < MAX_CLIENTS + 1; i++)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    nfds = 1;

    printf("[SERVER] is ready and wait connections on %s...\n", SOCKET_PATH);
    printf("[SERVER] enter Ctrl+c for stopping\n");

    while (!stop_server)
    {
        int poll_result = poll(fds, nfds, 1000);
        if (poll_result == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("poll");
            exit_code = EXIT_FAILURE;
            break;
        }
        if (poll_result == 0)
        {
            continue;
        }

        for (i = 0; i < nfds && !stop_server; i++)
        {
            short events = fds[i].revents;
            if (fds[i].fd == -1 || events == 0)
            {
                continue;
            }
            if (i == 0)
            {
                if (events & POLLIN)
                {
                    int client_fd = accept(server_fd, NULL, NULL);
                    if (client_fd == -1)
                    {
                        if (errno != EINTR)
                        {
                            perror("accept");
                        }
                    }
                    else
                    {
                        nfds_t slot;
                        for (slot = 1; slot <= MAX_CLIENTS; slot++)
                        {
                            if (fds[slot].fd == -1)
                                break;
                        }
                        if (slot > MAX_CLIENTS)
                        {
                            printf("[SERVER] max clients count\n");
                            close(client_fd);
                        }
                        else
                        {
                            fds[slot].fd = client_fd;
                            fds[slot].events = POLLIN;
                            fds[slot].revents = 0;

                            if (slot >= nfds)
                                nfds = slot + 1;
                            clients_count++;

                            printf("[SERVER] new client connect "
                                   "(fd=%d), total: %d\n",
                                   client_fd, clients_count);
                        }
                    }
                }
                if (events & (POLLERR | POLLHUP || POLLNVAL))
                {
                    fprintf(stderr, "[SERVER] server socket error\n");
                    exit_code = EXIT_FAILURE;
                    stop_server = 1;
                }
                continue;
            }
            if (events & POLLIN)
            {
                char buffer[BUFFER_SIZE];
                ssize_t bytes_read = read(fds[i].fd, buffer, sizeof(buffer));
                if (bytes_read > 0)
                {
                    for (ssize_t j = 0; j < bytes_read; j++)
                    {
                        buffer[j] = (char)toupper((unsigned char)buffer[j]);
                    }
                    printf("[CLIENT %d]: ", fds[i].fd);
                    fwrite(buffer, 1, (size_t)bytes_read, stdout);
                    fflush(stdout);
                }
                else if (bytes_read == 0)
                {
                    disconnect_client(fds, &nfds, i, &clients_count);
                }
                else
                {
                    if (errno != EINTR)
                    {
                        perror("read");
                        disconnect_client(fds, &nfds, i, &clients_count);
                    }
                }
            }
            else if (events & (POLLERR | POLLHUP | POLLNVAL))
            {
                disconnect_client(fds, &nfds, i, &clients_count);
            }
        }
    }
    printf("\n[SERVER] ending work...\n");
    for (i = 1; i <= MAX_CLIENTS; i++)
    {
        if (fds[i].fd != -1)
        {
            close(fds[i].fd);
            fds[i].fd = -1;
        }
    }

    close(server_fd);
    if (unlink(SOCKET_PATH) == -1 && errno != ENOENT)
        perror("unlink");
    printf("[SERVER] Server work is finished \n");

    return exit_code;
}