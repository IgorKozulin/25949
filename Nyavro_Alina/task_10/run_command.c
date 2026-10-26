#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // 1. Проверка аргументов
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 2. Создание подпроцесса
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // 3. Дочерний процесс: запускаем команду
        execvp(argv[1], &argv[1]);

        //Сюда попадём только если команда не найдена
        perror("execvp");
        exit(127);
    } 
    else {
        // 4. Родительский процесс: ждём и печатаем код завершения
        printf("[PARENT] Starting command: %s\n", argv[1]);
        fflush(stdout);

        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status)) {
            printf("[PARENT] Command exited normally with code: %d\n",
                   WEXITSTATUS(status));
        } 
        else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            printf("[PARENT] Command terminated by signal: %d (%s)\n",
                   sig, strsignal(sig));
        }
    }

    return 0;
}