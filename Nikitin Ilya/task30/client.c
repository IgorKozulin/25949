#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./socket"
#define BUFFER_SIZE 1024

int main(void)
{
    ssize_t bytes_read;
    int sock_fd;
    struct sockaddr_un address;
    char buffer[BUFFER_SIZE];
    signal(SIGPIPE, SIG_IGN);

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd == -1)
    {
        perror("scoket");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    if (connect(sock_fd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        perror("connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("connect to server\n");
    printf("Enter text (empty string or CTRL+D for exit):\n");

    while (fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        if (strcmp(buffer, "\n") == 0)
        {
            break;
        }

        size_t len = strlen(buffer);
        size_t written = 0;

        printf("[CLIENT] send: %s", buffer);

        while (written < len)
        {
            ssize_t result = write(sock_fd, buffer + written, len - written);

            if (result == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                perror("write");
                close(sock_fd);
                return EXIT_FAILURE;
            }
            if (result == 0)
                break;
            written += (size_t)result;
        }
        if (written < len)
            break;

        bytes_read = read(sock_fd, buffer, sizeof(buffer));
        if (bytes_read == -1)
        {
            if (errno == EINTR)
                continue;
            perror("read");
            close(sock_fd);
            return EXIT_FAILURE;
        }
        if (bytes_read == 0)
        {
            printf("[CLIENT] server disconnectedd\n");
            break;
        }
        printf("[CLIENT] Received: ");
        write(STDOUT_FILENO, buffer, bytes_read);
    }

    printf("[CLIENT] Close connection...\n");

    close(sock_fd);

    return EXIT_SUCCESS;
} // сервер должен отправлять текст назад и клиент его выводит в терминал
