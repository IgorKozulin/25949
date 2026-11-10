#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

int main(void)
{
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (pid == 0) // потомок -пишет в pipe
    {
        close(pipefd[0]);
        char buffer[BUFFER_SIZE];
        printf("[Child] Enter text (Ctrl+D or empty string to end)\n");

        while (fgets(buffer, sizeof(buffer), stdin) != NULL)
        {
            if (strcmp(buffer, "\n") == 0)
                break;
            size_t len = strlen(buffer);
            size_t written = 0;
            while (written < len)
            {
                size_t res = write(pipefd[1], buffer + written, len - written);
                if (res == -1)
                {
                    perror("write");
                    close(pipefd[1]);
                    exit(EXIT_FAILURE);
                }
                written += (size_t)res;
            }
        }
        close(pipefd[1]);
        exit(EXIT_SUCCESS);
    }

    // родитель - читает из pipe
    close(pipefd[1]);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0)
        write(STDOUT_FILENO, buffer, bytes_read);
    if (bytes_read == -1)
    {
        perror("read");
    }
    close(pipefd[0]);
    wait(NULL);

    return EXIT_SUCCESS;
} // поменят напралвение потомок пишет в pipe родитель читает из pipe без преобразования