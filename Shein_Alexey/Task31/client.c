#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_all(int fd, const void *data, size_t length)
{
    const unsigned char *p = data;

    while (length > 0) {
        ssize_t n = write(fd, p, length);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (n == 0) {
            errno = EIO;
            return -1;
        }

        p += n;
        length -= (size_t)n;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_un address;
    struct sigaction action;
    int fd;
    int status = EXIT_SUCCESS;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s socket_path\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strlen(argv[1]) >= sizeof(address.sun_path)) {
        fprintf(stderr, "Socket path is too long\n");
        return EXIT_FAILURE;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_IGN;

    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, argv[1]);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr *)&address,
                sizeof(address)) == -1) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }

    if (isatty(STDIN_FILENO)) {
        fprintf(stderr,
                "[CLIENT] Connected. Enter text; Ctrl+D to finish.\n");
    }

    for (;;) {
        unsigned char buffer[4096];
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("stdin");
            status = EXIT_FAILURE;
            break;
        }

        if (n == 0) {
            break;
        }

        if (write_all(fd, buffer, (size_t)n) == -1) {
            perror("write to socket");
            status = EXIT_FAILURE;
            break;
        }
    }

    if (close(fd) == -1) {
        perror("close");
        status = EXIT_FAILURE;
    }

    return status;
}
