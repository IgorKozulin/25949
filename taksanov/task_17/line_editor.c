#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINE_LEN 40

static struct termios orig_termios;
static char erase_char;
static char kill_char;

// Функция восстановления обычного режима терминала при выходе
void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Функция включения "сырого" режима терминала (Raw Mode)
void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    // Запоминаем системные клавиши стирания
    erase_char = orig_termios.c_cc[VERASE];
    kill_char = orig_termios.c_cc[VKILL];

    // Гарантируем, что терминал вернется в норму при любом выходе
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    // Отключаем ECHO (эхо), ICANON (канонический режим), IEXTEN (расширенные спецсимволы)
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);

    // Читаем по 1 байту без ожидания
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

// Удаление одного символа с экрана и из буфера
void erase_one_char(char *line, int *len) {
    if (*len > 0) {
        write(STDOUT_FILENO, "\b \b", 3);
        (*len)--;
    }
}

int main(void) {
    enable_raw_mode();

    char line[128];
    int len = 0;
    char c;

    while (read(STDIN_FILENO, &c, 1) == 1) {
        // 1. Выход по CTRL-D (код 4) только если курсор в начале строки
        if (c == 4) {
            if (len == 0) {
                write(STDOUT_FILENO, "\r\nExiting...\r\n", 14);
                break;
            } else {
                // Если курсор не в начале строки — звоним в колокольчик
                write(STDOUT_FILENO, "\a", 1);
                continue;
            }
        }

        // 2. Обработка ERASE (Backspace / Del / системный символ)
        if (c == erase_char || c == 127 || c == '\b') {
            erase_one_char(line, &len);
            continue;
        }

        // 3. Обработка KILL (Ctrl+U / системный символ) — стереть всю текущую строку
        if (c == kill_char || c == 21) {
            while (len > 0) {
                erase_one_char(line, &len);
            }
            continue;
        }

        // 4. Обработка CTRL-W (код 23) — стереть последнее слово и пробелы за ним
        if (c == 23) {
            // Сначала стираем пробелы в конце
            while (len > 0 && line[len - 1] == ' ') {
                erase_one_char(line, &len);
            }
            // Затем стираем само слово
            while (len > 0 && line[len - 1] != ' ') {
                erase_one_char(line, &len);
            }
            continue;
        }

        // 5. Обработка Enter (\n или \r) — перенос строки
        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\r\n", 2);
            len = 0;
            continue;
        }

        // 6. Печатаемые символы (включая пробел)
        if (isprint((unsigned char)c)) {
            // Если строка еще не достигла лимита в 40 символов
            if (len < MAX_LINE_LEN) {
                line[len++] = c;
                write(STDOUT_FILENO, &c, 1);
            } else {
                // Строка достигла 40 символов! Проверяем перенос слова (Word Wrap)
                if (c == ' ') {
                    // Если введен пробел на границе 40 символов — просто переходим на новую строку
                    write(STDOUT_FILENO, "\r\n", 2);
                    len = 0;
                } else {
                    // Ищем, где началось текущее слово
                    int word_start = len;
                    while (word_start > 0 && line[word_start - 1] != ' ') {
                        word_start--;
                    }

                    if (word_start == 0) {
                        // Если вся строка из 40 символов — одно длинное слово, переносим остаток
                        write(STDOUT_FILENO, "\r\n", 2);
                        line[0] = c;
                        len = 1;
                        write(STDOUT_FILENO, &c, 1);
                    } else {
                        // Слово началось в середине строки. Стираем его с текущей строки
                        int part_len = len - word_start;
                        for (int i = 0; i < part_len; i++) {
                            write(STDOUT_FILENO, "\b \b", 3);
                        }
                        // Переходим на следующую строку
                        write(STDOUT_FILENO, "\r\n", 2);

                        // Переносим слово на новую строку
                        char word_buf[64];
                        memcpy(word_buf, &line[word_start], part_len);
                        word_buf[part_len] = c;
                        len = part_len + 1;
                        memcpy(line, word_buf, len);

                        // Отображаем перенесенное слово
                        write(STDOUT_FILENO, line, len);
                    }
                }
            }
            continue;
        }

        // 7. Все остальные непечатаемые символы издают звуковой сигнал CTRL-G (\a)
        write(STDOUT_FILENO, "\a", 1);
    }

    return 0;
}
