#include <stdio.h>        // printf, snprintf
#include <stdlib.h>       // exit, EXIT_SUCCESS
#include <signal.h>       // sigaction, struct sigaction, SIGINT, SIGQUIT
#include <unistd.h>       // write, pause, STDOUT_FILENO, _exit
#include <string.h>       // strlen (опционально)

volatile sig_atomic_t beep_count = 0;

void sigint_handler()
{
    write(STDOUT_FILENO, "\a", 1);
    beep_count++;
}

void sigquit_handler()
{
    char buffer[100];
    int n = snprintf(buffer, sizeof(buffer), "\nВсего прозвучало сигналов: %d\n Программа завершена по SIGQUIT\n", (int)beep_count);
    if (n > 0)
    {
        write(STDOUT_FILENO, buffer, (size_t)n);
    }
    _exit(EXIT_SUCCESS);
}

int main()
{
    struct sigaction sa_int;
    struct sigaction sa_quit;

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction SIGINT");
        return EXIT_FAILURE;
    }

    memset(&sa_quit, 0, sizeof(sa_quit));
    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    if (sigaction(SIGQUIT, &sa_quit, NULL) == -1)
    {
        perror("sigaction SIGQUIT");
        return EXIT_FAILURE;
    }

    printf("Пищалка запущена!\n");
    printf("Нажмите Ctrl+C — будет звуковой сигнал\n");
    printf("Нажмите Ctrl+\\ — программа завершится с подсчётом сигналов\n");
    printf("Ожидаю сигналов...\n");
    fflush(stdout);

    while(1) pause();
    return EXIT_SUCCESS;
}
