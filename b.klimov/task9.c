/* task9.c */
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
        fprintf(stderr, "Использование: %s <путь_к_файлу>\n", argv[0]);
        return 1;
    }

    printf("Родительский процесс (PID: %ld): подготавливаю запуск cat...\n", (long)getpid());
    fflush(stdout);

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Дочерний процесс */
        execl("/bin/cat", "cat", argv[1], (char *)NULL);
        
        /* Если прямой путь не сработал, пробуем через PATH */
        execlp("cat", "cat", argv[1], (char *)NULL);

        /* Сюда попадаем только в случае ошибки exec */
        perror("execl/execlp");
        _exit(127);
    }

    /* Код родителя: выполняется параллельно, пока cat выводит файл */
    printf("Родительский процесс: потомок запущен с PID %ld. Выполняю свою работу.\n", (long)pid);
    fflush(stdout);

    /* Ожидаем завершения дочернего процесса через wait(2) */
    if (wait(&status) == -1) {
        perror("wait");
        return 1;
    }

    /* Гарантированно последняя строка родителя */
    if (WIFEXITED(status)) {
        printf("Родительский процесс: потомок завершил работу с кодом %d. Завершаю работу.\n", 
               WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Родительский процесс: потомок убит сигналом %d. Завершаю работу.\n", 
               WTERMSIG(status));
    } else {
        printf("Родительский процесс: потомок завершился нештатно. Завершаю работу.\n");
    }

    return 0;
}

/*
gcc -Wall -Wextra task9.c -o task9
./task9 /etc/passwd
*/