
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <aio.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./socket"
#define MAX_CLIENTS 100

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    keep_running = 0;
}

struct client
{
    int fd;
    struct aiocb cb;
    char byte;
    int active;
    int id;
};

static void disconnect_client(struct client *client)
{
    if (client->fd == -1)
        return;

    printf("\n[SERVER] client %d dissconect ", client->id);

    close(client->fd);

    client->fd = -1;
    client->active = 0;
    client->id = 0;

    printf("slot is free\n");
}

static int arm_read(struct client *client)
{
    memset(&client->cb, 0, sizeof(client->cb));

    client->cb.aio_fildes = client->fd;
    client->cb.aio_buf = &client->byte;
    client->cb.aio_nbytes = 1;
    client->cb.aio_offset = 0;

    client->cb.aio_sigevent.sigev_notify = SIGEV_NONE;

    if (aio_read(&client->cb) == -1)
    {
        if (errno == EAGAIN || errno == EINPROGRESS)
        {
            client->active = 1;
            return 0;
        }

        perror("[SERVER] aio_read");
        return -1;
    }

    client->active = 1;

    return 0;
}

int main(void)
{
    int server_fd;
    int client_id = 0;

    struct sockaddr_un server_addr;

    struct client clients[MAX_CLIENTS];

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1)
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

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sun_family = AF_UNIX;
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

    int flags = fcntl(server_fd, F_GETFL, 0);

    if (flags == -1 || fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
        clients[i].active = 0;
        clients[i].id = 0;
    }

    printf("[SERVER] AIO server is ready");
    printf("[SERVER] ctrcl+c for end \n");

    while (keep_running)
    {
        while (keep_running)
        {
            int client_fd = accept(server_fd, NULL, NULL);

            if (client_fd == -1)
            {
                if (errno == EINTR)
                    continue;

                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;

                perror("accept");
                break;
            }

            int client_flags = fcntl(client_fd, F_GETFL, 0);

            if (client_flags == -1 || fcntl(client_fd, F_SETFL, client_flags & ~O_NONBLOCK) == -1)
            {
                perror("fcntl client");
                close(client_fd);
                continue;
            }

            int slot;

            for (slot = 0; slot < MAX_CLIENTS; slot++)
            {
                if (clients[slot].fd == -1)
                    break;
            }

            if (slot == MAX_CLIENTS)
            {
                printf("[SERVER] maximum clients \n");
                close(client_fd);
                continue;
            }

            clients[slot].fd = client_fd;
            clients[slot].id = ++client_id;
            clients[slot].active = 0;

            printf("[SERVER] client %d connect "
                   "(slot=%d fd=%d).\n",
                   clients[slot].id,
                   slot,
                   client_fd);

            if (arm_read(&clients[slot]) == -1)
            {
                disconnect_client(&clients[slot]);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            struct client *client = &clients[i];

            if (client->fd == -1 || !client->active)
                continue;

            int error = aio_error(&client->cb);

            if (error == EINPROGRESS || error == EAGAIN)
                continue;

            if (error != 0)
            {
                if (error != ECANCELED)
                {
                    fprintf(stderr,
                            "[SERVER] error AIO client %d: %s\n",
                            client->id,
                            strerror(error));
                }

                aio_return(&client->cb);

                disconnect_client(client);
                continue;
            }

            ssize_t bytes_read = aio_return(&client->cb);

            client->active = 0;

            if (bytes_read > 0)
            {

                client->byte = (char)toupper(
                    (unsigned char)client->byte);

                if (write(STDOUT_FILENO, &client->byte, 1) == -1)
                {
                    perror("write");
                    disconnect_client(client);
                    continue;
                }

                if (arm_read(client) == -1)
                {
                    disconnect_client(client);
                }
            }
            else if (bytes_read == 0)
            {
                disconnect_client(client);
            }
            else
            {
                perror("aio_return");
                disconnect_client(client);
            }
        }

        usleep(10000);
    }

    printf("\n[SERVER] end work...\n");

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd != -1)
        {
            if (clients[i].active)
            {
                aio_cancel(clients[i].fd, &clients[i].cb);

                int error;

                do
                {
                    error = aio_error(&clients[i].cb);

                    if (error == EINPROGRESS || error == EAGAIN)
                        usleep(10000);

                } while (error == EINPROGRESS || error == EAGAIN);

                aio_return(&clients[i].cb);
            }

            close(clients[i].fd);

            clients[i].fd = -1;
            clients[i].active = 0;
        }
    }

    close(server_fd);

    if (unlink(SOCKET_PATH) == -1 && errno != ENOENT)
        perror("unlink");

    printf("[SERVER] work is done\n");

    return EXIT_SUCCESS;
}
