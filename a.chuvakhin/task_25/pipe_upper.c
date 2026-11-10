#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>

int main(void)
{
    int pipefd[2];
    char buf[256];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        close(pipefd[1]);
        printf("[CHILD] Запущен. Ожидаю данные от родителя...\n");
        fflush(stdout);

        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
            for (ssize_t i = 0; i < n; i++)
                buf[i] = toupper((unsigned char)buf[i]);
            buf[n] = '\0';
            printf("[CHILD] Получено и преобразовано: %s", buf);
            fflush(stdout);
        }

        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    }

    close(pipefd[0]);
    printf("[PARENT] Связь через канал установлена!\n");
    printf("[PARENT] Введите текст (пустая строка или Ctrl+D - выход):\n\n");

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;
        if (buf[0] == '\n') {
            printf("[PARENT] Пустая строка - завершаю ввод.\n");
            break;
        }
        write(pipefd[1], buf, strlen(buf));
        usleep(10000); 
    }

    close(pipefd[1]);
    wait(NULL);
    printf("[PARENT] Потомок завершился. Родитель тоже завершает работу.\n");
    return 0;
}
