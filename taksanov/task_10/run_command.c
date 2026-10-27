#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[]) {
    // 1. Проверяем аргументы
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return 1;
    }

    printf("[PARENT] Starting command: %s\n", argv[1]);

    // 2. Создаем дочерний процесс
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // --- ДОЧЕРНИЙ ПРОЦЕСС ---
        // Передаем argv[1] как имя команды, и &argv[1] как массив аргументов
        execvp(argv[1], &argv[1]);

        // Если execvp вернул управление — значит произошла ошибка (команда не найдена)
        perror("Execvp failed");
        exit(127); // Стандартный код в Unix для "command not found"
    }

    // --- РОДИТЕЛЬСКИЙ ПРОЦЕСС ---
    int status;
    // Ожидаем завершения именно нашего дочернего процесса
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid failed");
        return 1;
    }

    // 3. Анализ статуса завершения
    if (WIFEXITED(status)) {
        // Завершился штатно (через return или exit)
        printf("[PARENT] Command exited normally with code: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        // Был убит сигналом (например, Segmentation fault)
        int sig = WTERMSIG(status);
        char *sig_str = strsignal(sig);
        printf("[PARENT] Command terminated by signal: %d (%s)\n", 
               sig, sig_str ? sig_str : "Unknown signal");
    } else {
        printf("[PARENT] Command terminated abnormally\n");
    }

    return 0;
}
