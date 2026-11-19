#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int write_all(int fd, const char *data, size_t length)
{
    while (length > 0) {
        ssize_t n = write(fd, data, length);

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

        data += n;
        length -= (size_t)n;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    const char *messages[] = {
        "hello from client\n",
        "this is a test message\n",
        "asynchronous io is awesome\n"
    };
    struct sockaddr_un address;
    struct sigaction action;
    char buffer[512];
    int fd;
    int i;
    int result = EXIT_SUCCESS;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s socket_path name\n", argv[0]);
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

    for (i = 0; i < 3; ++i) {
        int length = snprintf(buffer, sizeof(buffer), "[%s] %s",
                              argv[2], messages[i]);

        if (length < 0 || (size_t)length >= sizeof(buffer)) {
            fprintf(stderr, "Client name is too long\n");
            result = EXIT_FAILURE;
            break;
        }

        if (write_all(fd, buffer, (size_t)length) == -1) {
            perror("write");
            result = EXIT_FAILURE;
            break;
        }

        if (i < 2) {
            struct timespec delay = {0, 500000000L};

            while (nanosleep(&delay, &delay) == -1) {
                if (errno != EINTR) {
                    perror("nanosleep");
                    close(fd);
                    return EXIT_FAILURE;
                }
            }
        }
    }

    if (close(fd) == -1) {
        perror("close");
        result = EXIT_FAILURE;
    }

    return result;
}
