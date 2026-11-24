#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

#define BUF_SIZE 4096

int main(void)
{
    int pipefd[2];
    pid_t pid;

    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0)
    {
        close(pipefd[1]);

        printf("[CHILD] Запущен. Ожидаю данные от родителя...\n");
        fflush(stdout);

        char buffer[BUF_SIZE];
        ssize_t n;

        while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0)
        {
            for (ssize_t i = 0; i < n; i++)
            {
                buffer[i] = (char)toupper((unsigned char)buffer[i]);
            }

            if (write(STDOUT_FILENO, buffer, (size_t)n) == -1)
            {
                perror("write stdout");
                break;
            }
        }

        if (n == -1)
        {
            perror("read");
        }

        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    }

    close(pipefd[0]);

    printf("[PARENT] Связь через канал установлена!\n");
    printf("[PARENT] Введите текст (пустая строка или Ctrl+D — выход):\n");
    fflush(stdout);

    char line[BUF_SIZE];
    while (1)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            printf("\n[PARENT] EOF — завершаю ввод.\n");
            break;
        }

        if (line[0] == '\n')
        {
            break;
        }

        size_t len = strlen(line);
        if (write(pipefd[1], line, len) != (ssize_t)len)
        {
            perror("write pipe");
            break;
        }
    }

    close(pipefd[1]);

    wait(NULL);
    printf("[PARENT] Потомок завершился. Родитель тоже завершает работу.\n");

    return EXIT_SUCCESS;
}
