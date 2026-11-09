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
    raw.c_cc[VMIN] = 1;  //читать по 1 байту
    raw.c_cc[VTIME] = 0; //без таймаута

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    raw_mode_enabled = 1;
    atexit(disable_raw_mode); //восстановить при выходе
}

static int last_lines = 0; //сколько строк занимал прошлый вывод (для стирания)

//Перерисовка с word wrap
static void redraw(const char *buf, int len) {
    //вернуться в начало прошлого вывода и стереть его
    if (last_lines > 0) {
        printf("\r\033[%dA\033[J", last_lines);
    } else {
        printf("\r\033[J");
    }
    last_lines = 0;

    int col = 0;
    int i = 0;
    int lines = 0; //сколько строк вывели (для следующего redraw)

    while (i < len) {
        int word_start = i;
        while (i < len && buf[i] != ' ') i++; //слово до пробела
        int word_len = i - word_start;

        //обычное слово (<= 40): переносим, если не влезает в остаток строки
        if (col > 0 && word_len <= 40 && col + word_len > 40) {
            putchar('\n');
            lines++;
            col = 0;
        }

        //слово > 40: один перенос (если не в начале строки), дальше пишем целиком
        if (col > 0 && word_len > 40) {
            putchar('\n');
            lines++;
            col = 0;
        }

        //печатаем слово целиком
        for (int j = word_start; j < i; j++) {
            putchar(buf[j]);
            col++;
        }

        //печатаем пробелы после слова
        while (i < len && buf[i] == ' ') {
            putchar(' ');
            i++;
            col++;
        }
    }

    fflush(stdout);
    last_lines = lines;
}

int main(void) {
    enable_raw_mode();

    char buffer[4096];
    int pos = 0;

    printf("> ");
    fflush(stdout);

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1); //читаем 1 байт
        if (n <= 0) break;

        //Ctrl+D (4) - выход, если строка пуста
        if (c == 4) {
            if (pos == 0) {
                printf("\nExiting...\n");
                break;
            }
            continue;
        }

        //Backspace (VERASE) - удалить последний символ
        if ((unsigned char)c == orig_termios.c_cc[VERASE]) {
            if (pos > 0) {
                pos--;
                redraw(buffer, pos);
            }
            continue;
        }

        //Ctrl+U (VKILL) - очистить строку
        if ((unsigned char)c == orig_termios.c_cc[VKILL]) {
            pos = 0;
            redraw(buffer, pos);
            continue;
        }

        //Ctrl+W (23) - удалить последнее слово и пробелы перед ним
        if (c == 23) {
            while (pos > 0 && buffer[pos - 1] == ' ') pos--; //сначала пробелы
            while (pos > 0 && buffer[pos - 1] != ' ') pos--; //потом слово
            redraw(buffer, pos);
            continue;
        }

        //Enter - новая строка
        if (c == '\n' || c == '\r') {
            printf("\n> ");
            fflush(stdout);
            pos = 0;
            last_lines = 0; //вывод зафиксирован, стирать больше нечего
            continue;
        }

        //Печатаемый символ - добавить в буфер и перерисовать
        if (isprint((unsigned char)c)) {
            if (pos < (int)sizeof(buffer) - 1) {
                buffer[pos++] = c;
                redraw(buffer, pos);
            }
            continue;
        }

        //Непечатаемый - звонок
        putchar('\a');
        fflush(stdout);
    }

    return 0;
}