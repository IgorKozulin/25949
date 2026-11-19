
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
        ssize_t bytes_written = write(fd, data + total, length - total);

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

int main(void)
{
    int client_fd;
    struct sockaddr_un server_addr;
    char buffer[BUFFER_SIZE];

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        perror("signal");
        return EXIT_FAILURE;
    }

    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (client_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SOCKET_PATH);

    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("connect");
        close(client_fd);
        return EXIT_FAILURE;
    }

    printf("[CLIENT] Connection is ready \n");
    printf("[CLIENT] Enter messahge\n");
    printf("[CLIENT] Empty string or Ctrl+D for exit\n");

    while (1)
    {
        printf("message> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL)
        {
            if (ferror(stdin))
                perror("fgets");

            break;
        }

        size_t length = strlen(buffer);

        if (length == 1 && buffer[0] == '\n')
            break;

        if (write_all(client_fd, buffer, length) == -1)
        {
            perror("write");
            break;
        }
    }

    printf("\n[CLIENT] Ending work\n");

    close(client_fd);

    return EXIT_SUCCESS;
}
