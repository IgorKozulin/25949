#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const char *name = "client";
    if (argc > 1) {
        name = argv[1];
    }

    const char *messages[] = {
        "hello from client\n",
        "this is a test message\n",
        "asynchronous io is awesome\n"
    };
    const size_t message_count = sizeof(messages) / sizeof(messages[0]);
    const struct timespec delay = { .tv_sec = 3 };

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    struct sockaddr_un address = {
        .sun_family = AF_UNIX,
        .sun_path = "./socket"
    };
    int result = 1;

    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("connect");
        goto done;
    }

    for (size_t i = 0; i < message_count; i++) {
        char buffer[1024];
        int length = snprintf(buffer, sizeof(buffer), "[%s] %s", name, messages[i]);
        if (length < 0 || (size_t)length >= sizeof(buffer)) {
            fputs("Name is too long\n", stderr);
            goto done;
        }

        size_t sent = 0;
        while (sent < (size_t)length) {
            ssize_t count = write(fd, buffer + sent, (size_t)length - sent);
            if (count == -1 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                perror("write");
                goto done;
            }
            sent += (size_t)count;
        }

        if (i + 1 < message_count) {
            nanosleep(&delay, NULL);
        }
    }
    result = 0;

done:
    close(fd);
    return result;
}
