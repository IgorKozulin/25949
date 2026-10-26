#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // 1. Проверяем, что передали хотя бы одну команду
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <команда> [аргументы...]\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Ошибка fork");
        return 1;
    }

    if (pid == 0) {
        // -----------------------------------------------------------
        // ДОЧЕРНИЙ ПРОЦЕСС
        // -----------------------------------------------------------
        // argv[1] - имя команды (например, "ls")
        // &argv[1] - массив аргументов, начиная с имени команды и до конца
        execvp(argv[1], &argv[1]);

        // Если execvp вернул управление — значит, команда не нашлась
        perror("Ошибка execvp");
        exit(127); // 127 - стандартный код Unix для "command not found"
    } else {
        // -----------------------------------------------------------
        // РОДИТЕЛЬСКИЙ ПРОЦЕСС
        // -----------------------------------------------------------
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("Ошибка waitpid");
            return 1;
        }

        // Анализируем, как именно завершился процесс
        if (WIFEXITED(status)) {
            // Процесс завершился сам (через exit или return)
            printf("[РОДИТЕЛЬ] Команда завершилась штатно. Код возврата: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            // Процесс был убит сигналом (segfault, SIGKILL и т.д.)
            int sig = WTERMSIG(status);
            printf("[РОДИТЕЛЬ] Команда аварийно убита сигналом %d (%s)\n", sig, strsignal(sig));
        } else {
            printf("[РОДИТЕЛЬ] Команда завершилась нетипичным образом.\n");
        }
    }

    return 0;
}
