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

    if (pid == 0) // потомок
    {
        close(pipefd[1]);
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0)
        {
            for (ssize_t i = 0; i < bytes_read; i++)
            {
                buffer[i] = (char)toupper((unsigned char)buffer[i]);
            }

            write(STDOUT_FILENO, buffer, bytes_read);
        }
        if (bytes_read == -1)
        {
            perror("read");
        }
        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    }

    // родитель
    close(pipefd[0]);
    char buffer[BUFFER_SIZE];
    printf("[Parent] Enter text (Ctrl+D or empty string ending):\n");
    while (fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
        if (strcmp(buffer, "\n") == 0)
        {
            break;
        }
        size_t len = strlen(buffer);
        size_t written = 0;
        while (written < len)
        {
            ssize_t result = write(pipefd[1], buffer + written, len - written);
            if (result == -1)
            {
                perror("write");
                close(pipefd[1]);
                wait(NULL);
                return EXIT_FAILURE;
            }
            written += (size_t)result;
        }
    }
    close(pipefd[1]);
    wait(NULL);

    return EXIT_SUCCESS;
}