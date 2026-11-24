#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    // Первый подпроцесс: пишет текст в канал
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("fork 1");
        close(fd[0]);
        close(fd[1]);
        return EXIT_FAILURE;
    }

    if (pid1 == 0) {
        close(fd[0]); // Конец чтения писателю не нужен

        const char *text =
            "Hello, World!\n"
            "Solaris Pipe Test: MiXeD cAsE 12345.\n"
            "Goodbye!\n";

        size_t total = strlen(text);
        size_t sent = 0;
        while (sent < total) {
            ssize_t w = write(fd[1], text + sent, total - sent);
            if (w == -1) {
                perror("write");
                close(fd[1]);
                _exit(EXIT_FAILURE);
            }
            sent += (size_t)w;
        }

        close(fd[1]); // Закрываем запись, чтобы второй процесс получил EOF
        _exit(EXIT_SUCCESS);
    }

    // Второй подпроцесс: читает из канала, переводит в верхний регистр и печатает
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("fork 2");
        close(fd[0]);
        close(fd[1]);
        waitpid(pid1, NULL, 0);
        return EXIT_FAILURE;
    }

    if (pid2 == 0) {
        close(fd[1]); // Обязательно закрываем конец записи у читателя

        char buf[256];
        ssize_t n;
        while ((n = read(fd[0], buf, sizeof(buf))) > 0) {
            for (ssize_t i = 0; i < n; i++) {
                buf[i] = (char)toupper((unsigned char)buf[i]);
            }
            ssize_t out = 0;
            while (out < n) {
                ssize_t w = write(STDOUT_FILENO, buf + out, (size_t)(n - out));
                if (w == -1) {
                    perror("write stdout");
                    close(fd[0]);
                    _exit(EXIT_FAILURE);
                }
                out += w;
            }
        }

        if (n == -1) {
            perror("read");
            close(fd[0]);
            _exit(EXIT_FAILURE);
        }

        close(fd[0]);
        _exit(EXIT_SUCCESS);
    }

    // Родитель закрывает оба конца канала и ждёт завершения обоих детей
    close(fd[0]);
    close(fd[1]);

    int status1 = 0, status2 = 0;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    if ((WIFEXITED(status1) && WEXITSTATUS(status1) != 0) ||
        (WIFEXITED(status2) && WEXITSTATUS(status2) != 0)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
