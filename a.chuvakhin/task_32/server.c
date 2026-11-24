#include <aio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 100
#define SOCKET_PATH "./socket"

struct client {
    int fd;
    unsigned char byte;
    struct aiocb request;
};

static volatile sig_atomic_t running = 1;

static void stop(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void close_client(struct client *client)
{
    close(client->fd);
    client->fd = -1;
}

static void start_read(struct client *client)
{
    client->request = (struct aiocb){
        .aio_fildes = client->fd,
        .aio_buf = &client->byte,
        .aio_nbytes = 1,
        .aio_sigevent = {
            .sigev_notify = SIGEV_SIGNAL,
            .sigev_signo = SIGUSR1
        }
    };
    if (aio_read(&client->request) == -1) {
        perror("aio_read");
        close_client(client);
    }
}

static void add_client(struct client clients[], int fd)
{
    int slot = 0;
    while (slot < MAX_CLIENTS && clients[slot].fd != -1) {
        slot++;
    }
    if (slot == MAX_CLIENTS) {
        close(fd);
        return;
    }

    if (fcntl(fd, F_SETFL, 0) == -1) {
        perror("fcntl");
        close(fd);
        return;
    }

    clients[slot].fd = fd;
    start_read(&clients[slot]);
}

int main(void)
{
    sigset_t notifications;
    sigemptyset(&notifications);
    sigaddset(&notifications, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &notifications, NULL) == -1) {
        perror("sigprocmask");
        return 1;
    }

    struct sigaction action = { .sa_handler = stop };
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) == -1) {
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

    struct client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
    }
    int result = 1;

    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        goto cleanup;
    }
    if (listen(server_fd, MAX_CLIENTS) == -1) {
        perror("listen");
        goto cleanup;
    }
    fprintf(stderr, "AIO server listening on %s. Ctrl+C to stop.\n", SOCKET_PATH);

    const struct timespec timeout = { .tv_nsec = 10000000 };
    while (running) {
        int fd = accept(server_fd, NULL, NULL);
        if (fd >= 0) {
            add_client(clients, fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            perror("accept");
            goto cleanup;
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            struct client *client = &clients[i];
            if (client->fd == -1) {
                continue;
            }

            int error = aio_error(&client->request);
            if (error == EINPROGRESS) {
                continue;
            }

            ssize_t count = aio_return(&client->request);
            if (count <= 0) {
                if (error > 0) {
                    errno = error;
                    perror("aio_read");
                }
                close_client(client);
                continue;
            }

            if (putchar(toupper(client->byte)) == EOF || fflush(stdout) == EOF) {
                perror("stdout");
                close_client(client);
                goto cleanup;
            }
            start_read(client);
        }

        if (fd == -1 && running) {
            sigtimedwait(&notifications, NULL, &timeout);
        }
    }
    result = 0;

cleanup:
    for (int i = 0; i < MAX_CLIENTS; i++) {
        struct client *client = &clients[i];
        if (client->fd == -1) {
            continue;
        }

        shutdown(client->fd, SHUT_RDWR);
        aio_cancel(client->fd, &client->request);
        const struct aiocb *requests[] = { &client->request };
        while (aio_error(&client->request) == EINPROGRESS) {
            aio_suspend(requests, 1, NULL);
        }
        aio_return(&client->request);
        close_client(client);
    }
    close(server_fd);
    unlink(SOCKET_PATH);
    return result;
}
