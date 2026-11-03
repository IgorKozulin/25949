/* task17.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define MAX_LINE 40
#define CTRL_D 4
#define CTRL_W 23
#define CTRL_G '\a'

static struct termios orig_termios;

/* Восстановление исходных настроек терминала при выходе */
void reset_terminal_mode(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

/* Удаление n символов с экрана */
void erase_chars_on_screen(int n) {
    for (int i = 0; i < n; i++) {
        write(STDOUT_FILENO, "\b \b", 3);
    }
}

int main(void) {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "Ошибка: стандартный ввод не является терминалом.\n");
        return 1;
    }

    /* Сохраняем исходные настройки терминала */
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(reset_terminal_mode);

    raw = orig_termios;
    /* Отключаем канонический режим и эхо */
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    cc_t erase_char = orig_termios.c_cc[VERASE];
    cc_t kill_char  = orig_termios.c_cc[VKILL];

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    char line_buf[MAX_LINE + 1];
    int line_len = 0;
    char ch;

    while (read(STDIN_FILENO, &ch, 1) == 1) {
        /* 5. CTRL-D в начале строки — завершение работы */
        if (ch == CTRL_D) {
            if (line_len == 0) {
                break;
            } else {
                char bell = CTRL_G;
                write(STDOUT_FILENO, &bell, 1);
                continue;
            }
        }

        /* Обработка нажатия Enter */
        if (ch == '\n' || ch == '\r') {
            write(STDOUT_FILENO, "\r\n", 2);
            line_len = 0;
            continue;
        }

        /* 2. Символ ERASE (обычно Backspace / DEL) */
        if ((unsigned char)ch == (unsigned char)erase_char || ch == '\b' || ch == 127) {
            if (line_len > 0) {
                line_len--;
                erase_chars_on_screen(1);
            }
            continue;
        }

        /* 3. Символ KILL — удаление всей строки */
        if ((unsigned char)ch == (unsigned char)kill_char) {
            if (line_len > 0) {
                erase_chars_on_screen(line_len);
                line_len = 0;
            }
            continue;
        }

        /* 4. Символ CTRL-W — стирание последнего слова и пробелов за ним */
        if (ch == CTRL_W) {
            if (line_len > 0) {
                int to_erase = 0;
                int pos = line_len;

                /* Пропускаем завершающие пробелы */
                while (pos > 0 && line_buf[pos - 1] == ' ') {
                    pos--;
                    to_erase++;
                }
                /* Пропускаем само слово */
                while (pos > 0 && line_buf[pos - 1] != ' ') {
                    pos--;
                    to_erase++;
                }

                line_len -= to_erase;
                erase_chars_on_screen(to_erase);
            }
            continue;
        }

        /* 6. Все остальные непечатаемые символы издают CTRL-G (звук) */
        if (!isprint((unsigned char)ch)) {
            char bell = CTRL_G;
            write(STDOUT_FILENO, &bell, 1);
            continue;
        }

        /* 1 & 7. Печатный символ: проверка длины строки (до 40 символов) */
        if (line_len == MAX_LINE) {
            /* Строка заполнена: ищем границу последнего слова для переноса */
            int word_start = line_len;
            while (word_start > 0 && line_buf[word_start - 1] != ' ') {
                word_start--;
            }

            if (word_start > 0 && word_start < line_len) {
                /* Найдено слово, пересекающее 40-й столбец: переносим его */
                int word_len = line_len - word_start;
                char wrap_word[MAX_LINE + 1];
                memcpy(wrap_word, &line_buf[word_start], word_len);

                /* Стираем переносимое слово с текущей строки */
                erase_chars_on_screen(word_len);

                /* Переход на новую строку */
                write(STDOUT_FILENO, "\r\n", 2);

                /* Записываем слово в начало новой строки */
                write(STDOUT_FILENO, wrap_word, word_len);
                memcpy(line_buf, wrap_word, word_len);
                line_len = word_len;
            } else {
                /* Вся строка — одно сплошное слово: переносим на новую строку */
                write(STDOUT_FILENO, "\r\n", 2);
                line_len = 0;
            }
        }

        /* Добавляем символ в буфер и выводим на экран */
        line_buf[line_len++] = ch;
        write(STDOUT_FILENO, &ch, 1);
    }

    return 0;
}

/*
# Компиляция
gcc -Wall -Wextra task17.c -o task17
./task17
# 1. Обычный ввод символов.
# 2. Backspace (ERASE) и Ctrl+U (KILL).
# 3. Ctrl+W — удаляет последнее слово.
# 4. Ввод строки длиннее 40 символов с пробелами (слово автоматически перенесется).
# 5. Ctrl+D в начале пустой строки — выход из программы.
*/