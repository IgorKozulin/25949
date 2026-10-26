#include <stdio.h>        // printf, perror
#include <stdlib.h>       // exit, EXIT_FAILURE
#include <unistd.h>       // fork, execlp, getpid
#include <sys/types.h>    // pid_t
#include <sys/wait.h>     // waitpid, WIFEXITED, WEXITSTATUS

int main(int argc, char *argv[])
{
    // 1. Проверка аргументов
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // 2. Создание подпроцесса
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // 3. Дочерний процесс
        printf("[CHILD] I am the child process. My PID: %d\n", getpid());
        fflush(stdout);

        execlp("cat", "cat", argv[1], NULL);

        // Сюда попадём только при ошибке execlp
        perror("execlp");
        exit(EXIT_FAILURE);
    } 
    else {
        // 4. Родительский процесс
        printf("[PARENT] Starting program. My PID: %d\n", getpid());
        printf("[PARENT] I am the parent. Child PID: %d\n", pid);
        printf("[PARENT] Printing some text while child is working...\n");
        fflush(stdout);

        // Ожидание завершения ребенка 
        printf("[PARENT] Waiting for child to finish...\n");
        fflush(stdout);

        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status)) {
            printf("[PARENT] Child exited with status: %d\n",
                   WEXITSTATUS(status));
        } else {
            printf("[PARENT] Child terminated abnormally\n");
        }

        // Финальная строка - строго после waitpid
        printf("[PARENT] Child has finished. "
               "This is the last line printed by parent.\n");
    }

    return 0;
}