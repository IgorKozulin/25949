#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define MAX_COLS 40

static struct termios orig_termios;
static int raw_mode_enabled = 0;
static int last_lines = 0;   //сколько экранных строк занимал прошлый вывод
static int last_cursor_row = 0; //на какой экранной строке (снизу) был курсор

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
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
    raw_mode_enabled = 1;
    atexit(disable_raw_mode);
}

//Перерисовка: вывести весь буфер, запомнить где курсор, вернуть терминальный курсор туда
static void redraw(const char *buf, int len, int pos) {
    //стереть прошлый вывод: вернуться в его начало
    if (last_lines > 0) {
        printf("\r\033[%dA\033[J", last_lines);
    } else {
        printf("\r\033[J");
    }

    int col = 0;       //колонка на текущей экранной строке
    int row = 0;       //номер экранной строки (сверху вниз)
    int lines = 0;     //всего экранных строк вывели
    int cur_row = 0;   //на какой экранной строке окажется курсор
    int cur_col = 0;   //в какой колонке

    int i = 0;
    while (i < len) {
        //запомнить позицию курсора, если он тут
        if (i == pos) { cur_row = row; cur_col = col; }

        int word_start = i;
        while (i < len && buf[i] != ' ') i++;
        int word_len = i - word_start;

        //обычное слово: перенос, если не влезает в остаток
        if (col > 0 && word_len <= MAX_COLS && col + word_len > MAX_COLS) {
            putchar('\n');
            row++; lines++;
            col = 0;
            if (i == pos) { cur_row = row; cur_col = col; }
        }
        //слово > MAX_COLS: один перенос, дальше целиком
        if (col > 0 && word_len > MAX_COLS) {
            putchar('\n');
            row++; lines++;
            col = 0;
            if (i == pos) { cur_row = row; cur_col = col; }
        }

        //печатаем слово посимвольно, проверяя pos
        for (int j = word_start; j < i; j++) {
            if (j == pos) { cur_row = row; cur_col = col; }
            putchar(buf[j]);
            col++;
        }

        //пробелы
        while (i < len && buf[i] == ' ') {
            if (i == pos) { cur_row = row; cur_col = col; }
            putchar(' ');
            i++;
            col++;
        }
    }

    //курсор в самом конце
    if (pos == len) { cur_row = row; cur_col = col; }

    fflush(stdout);

    //переместить терминальный курсор в (cur_row, cur_col)
    int up = lines - cur_row;
    if (up > 0) printf("\033[%dA", up);
    printf("\r");
    if (cur_col > 0) printf("\033[%dC", cur_col);
    fflush(stdout);

    last_lines = lines;
    (void)last_cursor_row;
}

int main(void) {
    enable_raw_mode();

    char buffer[4096];
    int len = 0;
    int pos = 0;

    printf("> ");
    fflush(stdout);

    while (1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;

        //Ctrl+D (4) - выход, если буфер пуст
        if (c == 4) {
            if (len == 0) {
                printf("\nExiting...\n");
                break;
            }
            continue;
        }

        //Backspace - удалить символ перед курсором
        if ((unsigned char)c == orig_termios.c_cc[VERASE] || c == 127) {
            if (pos > 0) {
                memmove(&buffer[pos - 1], &buffer[pos], len - pos);
                len--;
                pos--;
                redraw(buffer, len, pos);
            }
            continue;
        }

        //Ctrl+U (VKILL) - очистить всю строку
        if ((unsigned char)c == orig_termios.c_cc[VKILL] || c == 21) {
            len = 0;
            pos = 0;
            redraw(buffer, len, pos);
            continue;
        }

        //Ctrl+W (23) - удалить слово перед курсором + пробелы
        if (c == 23) {
            int new_pos = pos;
            while (new_pos > 0 && buffer[new_pos - 1] == ' ') new_pos--;
            while (new_pos > 0 && buffer[new_pos - 1] != ' ') new_pos--;
            int removed = pos - new_pos;
            if (removed > 0) {
                memmove(&buffer[new_pos], &buffer[pos], len - pos);
                len -= removed;
                pos = new_pos;
                redraw(buffer, len, pos);
            }
            continue;
        }

        //Стрелки: ESC [ A/B/C/D
        if (c == 27) {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'D': //влево
                        if (pos > 0) { pos--; redraw(buffer, len, pos); }
                        break;
                    case 'C': //вправо
                        if (pos < len) { pos++; redraw(buffer, len, pos); }
                        break;
                    case 'H': //Home
                        if (pos != 0) { pos = 0; redraw(buffer, len, pos); }
                        break;
                    case 'F': //End
                        if (pos != len) { pos = len; redraw(buffer, len, pos); }
                        break;
                }
            }
            continue;
        }

        //Enter - новая строка
        if (c == '\n' || c == '\r') {
            printf("\n> ");
            fflush(stdout);
            len = 0;
            pos = 0;
            last_lines = 0;
            continue;
        }

        //Печатаемый символ - вставить в позицию курсора
        if (isprint((unsigned char)c)) {
            if (len < (int)sizeof(buffer) - 1) {
                memmove(&buffer[pos + 1], &buffer[pos], len - pos);
                buffer[pos] = c;
                len++;
                pos++;
                redraw(buffer, len, pos);
            }
            continue;
        }

        putchar('\a'); 
        fflush(stdout);
    }
    return 0;
}