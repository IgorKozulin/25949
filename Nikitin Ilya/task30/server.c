#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "./socket"
#define BUFFER_SIZE 1024

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_un address;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    unlink(SOCKET_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        perror("blind");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 5) == -1)
    {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    printf("Server is ready. Waiting for connections...\n");
    fflush(stdout);

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1)
    {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    printf("Client has connected!\n");
    fflush(stdout);

    while (1)
    {
        bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read == -1)
        {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }
        if (bytes_read == 0)
            break;

        for (ssize_t i = 0; i < bytes_read; i++)
        {
            buffer[i] = (char)toupper((unsigned char)buffer[i]);
        }

        size_t written = 0;
        while (written < (size_t)bytes_read)
        {
            ssize_t res = write(client_fd, buffer + written, (size_t)bytes_read - written);
            if (res == -1)
            {
                if (errno == EINTR)
                    continue;
                perror("write");
                break;
            }
            written += (size_t)res;
        }
        if(written<(size_t)bytes_read)
            break;
    }
    printf("\nClient had disconected.\n");
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);
    printf("Server has finished the work\n");
    return EXIT_SUCCESS;
}
