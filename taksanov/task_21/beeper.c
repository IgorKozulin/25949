#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t beep_count = 0;
static volatile sig_atomic_t got_sigquit = 0;

void handle_sigint(int sig) {
    (void)sig;
    beep_count++;
    write(STDOUT_FILENO, "\a", 1); // Звуковой сигнал (BEL)
}

void handle_sigquit(int sig) {
    (void)sig;
    got_sigquit = 1;
}

int main(void) {
    struct sigaction sa_int, sa_quit;
    sigset_t mask, oldmask;

    // Настройка обработчика SIGINT (Ctrl+C)
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction SIGINT");
        return EXIT_FAILURE;
    }

    // Настройка обработчика SIGQUIT (Ctrl+\)
    memset(&sa_quit, 0, sizeof(sa_quit));
    sa_quit.sa_handler = handle_sigquit;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    if (sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
        perror("sigaction SIGQUIT");
        return EXIT_FAILURE;
    }

    // Блокируем SIGINT и SIGQUIT, чтобы безопасно ждать их через sigsuspend
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGQUIT);
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    while (!got_sigquit) {
        sigsuspend(&oldmask);
    }

    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    printf("\nSounded %d time(s).\n", (int)beep_count);
    return EXIT_SUCCESS;
}
