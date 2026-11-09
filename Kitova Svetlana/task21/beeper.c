#include <stdio.h>        // printf, snprintf
#include <stdlib.h>       // exit, EXIT_SUCCESS
#include <signal.h>       // sigaction, struct sigaction, SIGINT, SIGQUIT
#include <unistd.h>       // write, pause, STDOUT_FILENO, _exit
#include <string.h>       // strlen (опционально)
#include <termios.h>

volatile sig_atomic_t beep_count = 0;
volatile sig_atomic_t quit_requested = 0;

// Обработчик SIGINT
void sigint_handler(int signal) {
    write(STDOUT_FILENO, "\a", 1);
    beep_count++;
}

// Обработчик SIGQUIT
void sigquit_handler(int signal) {
    quit_requested = 1;
}

int main(void) {
    struct sigaction sa_int;
    struct sigaction sa_quit;
    struct termios old_term;
    struct termios new_term;

    // Получаем текущие настройки терминала
    tcgetattr(STDIN_FILENO, &old_term);

    // Копируем настройки
    new_term = old_term;

    // Отключаем отображение управляющих символов
    new_term.c_lflag &= ~ECHOCTL;

    // Применяем новые настройки
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    // Настраиваем обработчик SIGINT
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;

    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("sigaction SIGINT");
        return EXIT_FAILURE;
    }

    // Настраиваем обработчик SIGQUIT
    sa_quit.sa_handler = sigquit_handler;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;

    if (sigaction(SIGQUIT, &sa_quit, NULL) == -1) {
        perror("sigaction SIGQUIT");
        return EXIT_FAILURE;
    }

    // Блокируем SIGQUIT, чтобы не потерять сигнал
    sigset_t block_mask;
    sigset_t old_mask;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGQUIT);

    if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    printf("Пищалка запущена!\n");
    printf("Нажмите Ctrl+C для звукового сигнала.\n");
    printf("Нажмите Ctrl+\\ для завершения.\n");
    fflush(stdout);

    // Основной цикл
    while (!quit_requested) {
        sigsuspend(&old_mask);
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    // Выводим результат после выхода из цикла
    printf("\nВсего прозвучало сигналов: %d\n", (int)beep_count);
    printf("Программа завершена.\n");

    return EXIT_SUCCESS;

}
