#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <aio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 32

struct Client {
    int fd;
    int active;
    unsigned char byte;
    struct aiocb request;
};

static struct Client clients[MAX_CLIENTS];

static int set_blocking(int fd, int blocking)
{
    int flags = fcntl(fd, F_GETFL);

    if (flags == -1) {
        return -1;
    }

    if (blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }

    return fcntl(fd, F_SETFL, flags);
}

static int start_read(struct Client *client)
{
    memset(&client->request, 0, sizeof(client->request));

    client->request.aio_fildes = client->fd;
    client->request.aio_buf = &client->byte;
    client->request.aio_nbytes = 1;
    client->request.aio_offset = 0;
    client->request.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    client->request.aio_sigevent.sigev_signo = SIGUSR1;

    if (aio_read(&client->request) == -1) {
        if (errno == EAGAIN) {
            return 0;
        }

        perror("aio_read");
        return -1;
    }

    client->active = 1;
    return 0;
}

static void close_client(struct Client *client)
{
    fprintf(stderr, "[SERVER] Client fd=%d disconnected\n",
            client->fd);
    close(client->fd);
    client->fd = -1;
}

static void cleanup_clients(void)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].fd != -1) {
            shutdown(clients[i].fd, SHUT_RDWR);
        }
    }

    for (i = 0; i < MAX_CLIENTS; ++i) {
        struct Client *client = &clients[i];

        if (client->fd == -1) {
            continue;
        }

        if (client->active) {
            const struct aiocb *list[] = {&client->request};

            aio_cancel(client->fd, &client->request);

            while (aio_error(&client->request) == EINPROGRESS) {
                if (aio_suspend(list, 1, NULL) == -1 &&
                    errno != EINTR) {
                    perror("aio_suspend");
                    _exit(EXIT_FAILURE);
                }
            }

            aio_return(&client->request);
            client->active = 0;
        }

        close(client->fd);
        client->fd = -1;
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_un address;
    sigset_t signals;
    int listener = -1;
    int bound = 0;
    int result = EXIT_FAILURE;
    int i;

    for (i = 0; i < MAX_CLIENTS; ++i) {
        clients[i].fd = -1;
    }

    if (argc != 2) {
        fprintf(stderr, "Usage: %s socket_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strlen(argv[1]) >= sizeof(address.sun_path)) {
        fprintf(stderr, "Socket path is too long\n");
        return EXIT_FAILURE;
    }

    if (sigemptyset(&signals) == -1 ||
        sigaddset(&signals, SIGUSR1) == -1 ||
        sigaddset(&signals, SIGINT) == -1 ||
        sigaddset(&signals, SIGTERM) == -1 ||
        sigprocmask(SIG_BLOCK, &signals, NULL) == -1) {
        perror("signal mask");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, argv[1]);

    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener == -1) {
        perror("socket");
        goto cleanup;
    }

    if (set_blocking(listener, 0) == -1) {
        perror("fcntl listener");
        goto cleanup;
    }

    if (bind(listener, (struct sockaddr *)&address,
             sizeof(address)) == -1) {
        perror("bind");
        goto cleanup;
    }
    bound = 1;

    if (listen(listener, MAX_CLIENTS) == -1) {
        perror("listen");
        goto cleanup;
    }

    fprintf(stderr, "[SERVER] AIO server on %s\n", argv[1]);
    fprintf(stderr, "[SERVER] Ctrl+C to stop\n");

    for (;;) {
        struct timespec timeout = {0, 20000000L};
        int received;
        int fd = accept(listener, NULL, NULL);

        if (fd != -1) {
            int slot;

            for (slot = 0; slot < MAX_CLIENTS; ++slot) {
                if (clients[slot].fd == -1) {
                    break;
                }
            }

            if (slot == MAX_CLIENTS) {
                fprintf(stderr, "[SERVER] Too many clients\n");
                close(fd);
            } else if (set_blocking(fd, 1) == -1) {
                perror("fcntl client");
                close(fd);
            } else {
                clients[slot].fd = fd;
                clients[slot].active = 0;

                fprintf(stderr, "[SERVER] Client connected, fd=%d\n",
                        fd);
            }
        } else if (errno != EAGAIN && errno != EWOULDBLOCK &&
                   errno != EINTR) {
            perror("accept");
            goto cleanup;
        }

        for (i = 0; i < MAX_CLIENTS; ++i) {
            struct Client *client = &clients[i];

            if (client->fd == -1) {
                continue;
            }

            if (client->active) {
                int error = aio_error(&client->request);
                ssize_t n;

                if (error == EINPROGRESS) {
                    continue;
                }

                if (error == -1) {
                    perror("aio_error");
                    goto cleanup;
                }

                n = aio_return(&client->request);
                client->active = 0;

                if (error != 0) {
                    fprintf(stderr, "AIO fd=%d: %s\n",
                            client->fd, strerror(error));
                    close_client(client);
                    continue;
                }

                if (n == 0) {
                    close_client(client);
                    continue;
                }

                if (n != 1) {
                    fprintf(stderr, "Unexpected AIO result\n");
                    close_client(client);
                    continue;
                }

                if (putchar(toupper(client->byte)) == EOF ||
                    fflush(stdout) == EOF) {
                    perror("stdout");
                    goto cleanup;
                }
            }

            if (start_read(client) == -1) {
                close_client(client);
                result = EXIT_FAILURE;
                goto cleanup;
            }
        }

        received = sigtimedwait(&signals, NULL, &timeout);

        if (received == SIGINT || received == SIGTERM) {
            result = EXIT_SUCCESS;
            break;
        }

        if (received == -1 && errno != EAGAIN && errno != EINTR) {
            perror("sigtimedwait");
            goto cleanup;
        }
    }

cleanup:
    cleanup_clients();

    if (listener != -1 && close(listener) == -1) {
        perror("close listener");
        result = EXIT_FAILURE;
    }

    if (bound && unlink(argv[1]) == -1) {
        perror("unlink");
        result = EXIT_FAILURE;
    }

    return result;
}
