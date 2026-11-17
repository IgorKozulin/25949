#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>

static volatile sig_atomic_t beep_count = 0;
static struct termios orig_termios;
static int tty_fd = -1;

static void restore_terminal(void)
{
    if (tty_fd != -1) {
        tcsetattr(tty_fd, TCSANOW, &orig_termios);
    }
}

static void sigint_handler(int signo)
{
    (void)signo;
    if (write(STDOUT_FILENO, "\a", 1) < 0) {}
    beep_count++;
}

static void sigquit_handler(int signo)
{
    (void)signo;
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer),
                       "\nВсего прозвучало сигналов: %d\n"
                       "Программа завершена по SIGQUIT\n",
                       (int)beep_count);
    if (len > 0) {
        if ((size_t)len >= sizeof(buffer))
            len = (int)sizeof(buffer) - 1;
        (void)write(STDOUT_FILENO, buffer, (size_t)len);
    }
    restore_terminal();
    _exit(EXIT_SUCCESS);
}

int main(void)
{
    struct sigaction sa_int;
    struct sigaction sa_quit;

    /* Открываем напрямую управляющий терминал /dev/tty */
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd != -1) {
        if (tcgetattr(tty_fd, &orig_termios) == 0) {
            struct termios raw = orig_termios;
            raw.c_lflag &= ~(tcflag_t)ECHOCTL;
            if (tcsetattr(tty_fd, TCSANOW, &raw) < 0) {
                perror("tcsetattr");
            }
        } else {
            perror("tcgetattr");
        }
    }

    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) < 0) {
        perror("sigaction(SIGINT)");
        restore_terminal();
        return EXIT_FAILURE;
    }

    memset(&sa_quit, 0, sizeof(sa_quit));
    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    if (sigaction(SIGQUIT, &sa_quit, NULL) < 0) {
        perror("sigaction(SIGQUIT)");
        restore_terminal();
        return EXIT_FAILURE;
    }

    printf("Пищалка запущена!\n");
    printf("Нажмите Ctrl+C - будет звуковой сигнал\n");
    printf("Нажмите Ctrl+\\ - программа завершится с подсчётом сигналов\n");
    printf("Ожидаю сигналов...\n");
    fflush(stdout);

    while (1) {
        pause();
    }

    return EXIT_SUCCESS;
}
