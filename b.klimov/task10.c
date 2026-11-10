/* task10.c */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    pid_t pid;
    int status;

    if (argc < 2) {
        fprintf(stderr, "Использование: %s <команда> [аргументы...]\n", argv[0]);
        return 1;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Передаем имя исполняемого файла и указатель на срез аргументов */
        execvp(argv[1], &argv[1]);

        /* Если execvp вернул управление, значит запуск не удался */
        perror("execvp");
        _exit(127);
    }

    /* Родитель ожидает завершения запущенной команды */
    if (waitpid(pid, &status, 0) == (pid_t)-1) {
        perror("waitpid");
        return 1;
    }

    /* Анализ кода завершения */
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        printf("Процесс %ld завершился нормально. Код возврата: %d\n", (long)pid, exit_code);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("Процесс %ld был прерван сигналом: %d\n", (long)pid, sig);
#ifdef WCOREDUMP
        if (WCOREDUMP(status)) {
            printf("Создан дамп памяти (core dumped).\n");
        }
#endif
    } else if (WIFSTOPPED(status)) {
        printf("Процесс %ld был остановлен сигналом: %d\n", (long)pid, WSTOPSIG(status));
    }

    return 0;
}

/*
gcc -Wall -Wextra task10.c -o task10
gcc -Wall -Wextra crash.c -o crash

# Проверка задачи 10 с нормальным завершением
./task10 ls -la /tmp

# Проверка задачи 10 с ненулевым кодом выхода
./task10 grep "not_found_string_xyz" /etc/passwd

# Проверка задачи 10 с падением по сигналу (SIGSEGV — сигнал 11)
./task10 ./crash

*/