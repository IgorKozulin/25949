#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

/* Надёжная запись всех данных в канал */
static ssize_t write_all(int fd, const void *buf, size_t count)
{
    const char *p = (const char *)buf;
    size_t left = count;

    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            break;
        p += n;
        left -= (size_t)n;
    }
    return (ssize_t)(count - left);
}

int main(void)
{
    int pipefd[2];
    pid_t pid;

    signal(SIGPIPE, SIG_IGN);

    //1. Создаём программный канал
    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    //2. Создаём дочерний процесс
    pid = fork();
    if (pid < 0) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        //3. Дочерний процесс: читает из канала и переводит в верхний регистр
        close(pipefd[1]);

        printf("[CHILD] Запущен. Ожидаю данные от родителя...\n");
        fflush(stdout);

        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;

        while (1) {
            bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
            if (bytes_read < 0) {
                if (errno == EINTR)
                    continue;
                perror("read");
                close(pipefd[0]);
                exit(EXIT_FAILURE);
            }
            if (bytes_read == 0) {
                break;
            }

            buffer[bytes_read] = '\0';

            for (ssize_t i = 0; i < bytes_read; i++) {
                buffer[i] = (char)toupper((unsigned char)buffer[i]);
            }

            printf("[CHILD] Получено и преобразовано: %s", buffer);
            fflush(stdout);
        }

        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    }

    // 4. Родительский процесс: читает ввод с клавиатуры и пишет в канал 
    close(pipefd[0]);

    printf("[PARENT] Связь через канал установлена!\n");
    printf("[PARENT] Введите текст (пустая строка или Ctrl+D — выход):\n\n");
    fflush(stdout);

    char input[BUFFER_SIZE];

    while (1) {
        printf("> ");
        fflush(stdout);

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

       int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

        if (ready < 0) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (ready == 0) {
            // Прошло 5 секунд, а пользователь так ничего и не ввёл
            printf("\n[PARENT] Время ожидания (5 сек) истекло — завершаю ввод.\n");
            break;
        }

        //Если ready > 0, значит данные появились - читаем их безопасно
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (ferror(stdin)) {
                perror("fgets");
            } else {
                printf("\n[PARENT] Получен EOF (Ctrl+D) — завершаю ввод.\n");
            }
            break;
        }

        //Выход по пустой строке (нажат только Enter)
        if (input[0] == '\n') {
            printf("[PARENT] Пустая строка — завершаю ввод.\n");
            break;
        }

        if (write_all(pipefd[1], input, strlen(input)) < 0) {
            perror("write");
            break;
        }

        usleep(10000);
    }

    close(pipefd[1]);

    if (wait(NULL) < 0) {
        perror("wait");
        exit(EXIT_FAILURE);
    }

    printf("[PARENT] Потомок завершился. Родитель тоже завершает работу.\n");
    return 0;
}
