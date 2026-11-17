#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>

/* Счётчик сигналов SIGINT.
volatile - запрещает кэширование в регистре.
sig_atomic_t - гарантирует, что процессор запишет или прочитает число за одно неделимое действие*/
static volatile sig_atomic_t beep_count = 0;
static struct termios orig_termios;

static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

static void sigint_handler(int signo)
{
    (void)signo;
    //"\a" — ASCII 7 (BEL). Пишем 1 байт в stdout
    if (write(STDOUT_FILENO, "\a", 1) < 0) {

    }
    beep_count++;
}

static void sigquit_handler(int signo)
{
    (void)signo;
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer),
                       "\n Всего прозвучало сигналов: %d\n"
                       "Программа завершена по SIGQUIT\n",
                       (int)beep_count);
    if (len > 0) {
        if ((size_t)len > sizeof(buffer))
            len = (int)sizeof(buffer);
        (void)write(STDOUT_FILENO, buffer, (size_t)len);
    }
    restore_terminal();
    _exit(EXIT_SUCCESS);
}

int main(void)
{
    struct sigaction sa_int;
    struct sigaction sa_quit;

    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        raw.c_lflag &= ~(tcflag_t)ECHOCTL;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler; //SIGINT, SIGQUIT
    sigemptyset(&sa_int.sa_mask); //никакие дополнительные сигналы не блокируются
    sa_int.sa_flags = SA_RESTART; //перезапускать pause() после сигнала 
    if (sigaction(SIGINT, &sa_int, NULL) < 0) {
        perror("sigaction(SIGINT)");
        return EXIT_FAILURE;
    }

    memset(&sa_quit, 0, sizeof(sa_quit));
    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    if (sigaction(SIGQUIT, &sa_quit, NULL) < 0) {
        perror("sigaction(SIGQUIT)");
        return EXIT_FAILURE;
    }

    printf("Пищалка запущена!\n");
    printf("Нажмите Ctrl+C - будет звуковой сигнал\n");
    printf("Нажмите Ctrl+\\ - программа завершится c подсчётом сигналов\n");
    printf("Ожидаю сигналов...\n");
    fflush(stdout);

    while (1) {
        pause();
    }

    return EXIT_SUCCESS;
}