#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile sig_atomic_t beep_count = 0;

void sigint_handler(int sig)
{
    (void)sig;
    write(STDOUT_FILENO, "\a", 1);
    beep_count++;
}

void sigquit_handler(int sig)
{
    char buffer[100];
    int len;
    (void)sig;

    len = snprintf(buffer, sizeof(buffer),
                   "\nВсего прозвучало сигналов: %d\n", (int)beep_count);
    write(STDOUT_FILENO, buffer, len);

    const char *msg = "Программа завершена по SIGQUIT\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    _exit(EXIT_SUCCESS);
}

int main(void)
{
    struct sigaction sa_int;
    struct sigaction sa_quit;

    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(EXIT_FAILURE);
    }

    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    if (sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
        perror("sigaction SIGQUIT");
        exit(EXIT_FAILURE);
    }

    printf("Нажмите Ctrl+C - будет звуковой сигнал\n");
    printf("Ожидаю сигналов...\n");
    fflush(stdout);

    while (1) {
        pause();
    }

    return 0;
}
