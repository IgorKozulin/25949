
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./socket"
#define BUFFER_SIZE 1024

static int write_all(int fd, const char *data, size_t length)
{
    size_t total = 0;

    while (total < length)
    {
        ssize_t bytes_written = write(
            fd,
            data + total,
            length - total);

        if (bytes_written > 0)
        {
            total += (size_t)bytes_written;
        }
        else if (bytes_written == -1 && errno == EINTR)
        {
            continue;
        }
        else
        {
            if (bytes_written == 0)
                errno = EIO;

            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int sock_fd;

    const char *name;

    struct sockaddr_un server_addr;

    char buffer[BUFFER_SIZE];

    const char *messages[] =
        {
            "hello from client\n",
            "this is a test message\n",
            "asynchronous io is awesome\n"};

    int messages_count = sizeof(messages) / sizeof(messages[0]);

    if (argc > 1)
        name = argv[1];
    else
        name = "client";

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        perror("signal");
        return EXIT_FAILURE;
    }

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sun_family = AF_UNIX;

    if (strlen(SOCKET_PATH) >= sizeof(server_addr.sun_path))
    {
        fprintf(stderr, "Too long path to socket\n");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    strcpy(server_addr.sun_path, SOCKET_PATH);

    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1)
    {
        perror("connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("[CLIENT] Connected to server. Name: %s\n", name);

    for (int i = 0; i < messages_count; i++)
    {
        int length = snprintf(
            buffer,
            sizeof(buffer),
            "[%s] %s",
            name,
            messages[i]);

        if (length < 0 || (size_t)length >= sizeof(buffer))
        {
            fprintf(stderr, "Message is too long\n");
            close(sock_fd);
            return EXIT_FAILURE;
        }

        if (write_all(sock_fd, buffer, (size_t)length) == -1)
        {
            perror("write");
            close(sock_fd);
            return EXIT_FAILURE;
        }

        printf("[CLIENT] Message %d sent\n", i + 1);

        if (i < messages_count - 1)
            usleep(500000);
    }

    printf("[CLIENT] All messages sent.\n");


    close(sock_fd);

    return EXIT_SUCCESS;
}
