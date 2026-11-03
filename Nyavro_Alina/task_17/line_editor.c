#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

static struct termios orig_termios; //исходные настройки терминала
static int raw_mode_enabled = 0;

//Raw mode 
static void disable_raw_mode(void) {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = 0;
    }
}

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN); //отключить эхо, канон, расширенную
    raw.c_cc[VMIN] = 1; //читать по 1 байту
    raw.c_cc[VTIME] = 0; //без таймаута

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    raw_mode_enabled = 1;
    atexit(disable_raw_mode); //восстановить при выходе
}

//Перерисовка с word wrap
static void redraw(const char *buf, int len) {
    printf("\r\033[K");

    int col = 0; //позиция символа в строке
    int i = 0;

    while (i < len) {
        int word_start = i;
        while (i < len && buf[i] != ' ') i++;
        int word_len = i - word_start;

        if (col + word_len > 40 && col > 0) {
            printf("\n\033[K");
            col = 0;
        }

        for (int j = word_start; j < i; j++) {
            putchar(buf[j]);
            col++;
        }

        while (i < len && buf[i] == ' ') {
            putchar(' ');
            i++;
            col++;
        }
    }

    fflush(stdout);
}

int main(void) {
    enable_raw_mode();

    char buffer[4096];
    int pos = 0;

    printf("> ");
    fflush(stdout);

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;

        //Ctrl+D (4) - выход, если строка пуста
        if (c == 4) {
            if (pos == 0) {
                printf("\nExiting...\n");
                break;
            }
            continue;
        }

        //Backspace (VERASE)
        if (c == orig_termios.c_cc[VERASE]) {
            if (pos > 0) {
                pos--;
                redraw(buffer, pos);
            }
            continue;
        }

        //Ctrl+U (VKILL) - очистить строку
        if (c == orig_termios.c_cc[VKILL]) {
            pos = 0;
            redraw(buffer, pos);
            continue;
        }

        //Ctrl+W (23) - удалить последнее слово
        if (c == 23) {
            while (pos > 0 && buffer[pos-1] == ' ') pos--;
            while (pos > 0 && buffer[pos-1] != ' ') pos--;
            redraw(buffer, pos);
            continue;
        }

        //Enter - новая строка
        if (c == '\n' || c == '\r') {
            printf("\n> ");
            fflush(stdout);
            pos = 0;
            continue;
        }

        //Печатаемый символ - добавить в буфер
        if (isprint((unsigned char)c)) {
            if (pos < (int)sizeof(buffer) - 1) {
                buffer[pos++] = c;
                redraw(buffer, pos);
            }
            continue;
        }

        //Непечатаемый - сигнал
        putchar('\a');
        fflush(stdout);
    }

    return 0;
}