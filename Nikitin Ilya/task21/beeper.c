#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

struct termios old_terminal;

void disable_echoctl(void)
{
    struct termios terminal;
    tcgetattr(STDIN_FILENO, &old_terminal);
    terminal = old_terminal;
    terminal.c_lflag &= ~ECHOCTL;
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal);
}

void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &old_terminal);
}

volatile sig_atomic_t beep_count = 0;

void sigint_handler(int sig)
{
    (void)sig;
    write(STDOUT_FILENO, "\a", 1);
    beep_count++;
}

void sigquit_handler(int sig)
{
    (void)sig;
    int count = beep_count;
    const char *message = "Всего прозвучало сигналов: ";
    write(STDOUT_FILENO, message, 28);
    if (count == 0)
    {
        write(STDOUT_FILENO, "0\n", 2);
    }
    else
    {
        char digits[20];
        int i = 0;
        while (count > 0)
        {
            digits[i++] = '0' + count % 10;
            count /= 10;
        }
        while (i > 0)
        {
            char c = digits[--i];
            write(STDOUT_FILENO, &c, 1);
        }
        write(STDOUT_FILENO, "\n", 1);
    }
    write(STDOUT_FILENO, "Программа завершена по SIGQUIT\n", 32);
    restore_terminal();
    _exit(EXIT_SUCCESS);
}

int main(void)
{
    struct sigaction sa_int;
    struct sigaction sa_quit;

    disable_echoctl();

    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
        perror("sigaction SIGINT");
        return EXIT_FAILURE;
    }

    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    if (sigaction(SIGQUIT, &sa_quit, NULL) == -1)
    {
        restore_terminal();
        perror("sigaction SIGQUIT");
        return EXIT_FAILURE;
    }

    printf("Пищалка запущена!\n");
    printf("Нажмите Ctrl+C — будет звуковой сигнал\n");
    printf("Нажмите Ctrl+\\ — программа завершится с подсчетом сигналов\n");
    printf("Ожидаю сигналов...\n");

    while (1)
    {
        pause();
    }

    return EXIT_FAILURE;
}