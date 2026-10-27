#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINE 40

static struct termios orig_termios;

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

void ring_bell(void) {
    putchar('\a');
    fflush(stdout);
}

void erase_one_char(void) {
    printf("\b \b");
}

int main(void) {
    enable_raw_mode();

    char erase_char = orig_termios.c_cc[VERASE];
    char kill_char = orig_termios.c_cc[VKILL];

    char line[MAX_LINE + 10];
    int len = 0;

    // Выводим приглашение, чтобы видеть, что программа активна!
    printf("> ");
    fflush(stdout);

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) {
            break;
        }

        // 1. CTRL-D: выход, только если строка пустая
        if (c == 4) {
            if (len == 0) {
                break;
            } else {
                ring_bell();
            }
            continue;
        }

        // 2. ERASE (Backspace / 127 / 8)
        if (c == erase_char || c == 127 || c == 8) {
            if (len > 0) {
                erase_one_char();
                len--;
                fflush(stdout);
            } else {
                ring_bell();
            }
            continue;
        }

        // 3. KILL (Ctrl+U / 21)
        if (c == kill_char || c == 21) {
            if (len > 0) {
                while (len > 0) {
                    erase_one_char();
                    len--;
                }
                fflush(stdout);
            } else {
                ring_bell();
            }
            continue;
        }

        // 4. CTRL-W (код 23): удалить слово
        if (c == 23) {
            if (len == 0) {
                ring_bell();
            } else {
                while (len > 0 && line[len - 1] == ' ') {
                    erase_one_char();
                    len--;
                }
                while (len > 0 && line[len - 1] != ' ') {
                    erase_one_char();
                    len--;
                }
                fflush(stdout);
            }
            continue;
        }

        // 5. Enter (\n или \r)
        if (c == '\n' || c == '\r') {
            putchar('\n');
            printf("> ");
            len = 0;
            fflush(stdout);
            continue;
        }

        // 6. Печатные символы (ASCII >= 32)
        if ((unsigned char)c >= 32 && (unsigned char)c != 127) {
            if (len < MAX_LINE) {
                line[len++] = c;
                putchar(c);
                fflush(stdout);
            } else {
                // Перенос строки при превышении 40 символов
                if (c == ' ') {
                    putchar('\n');
                    printf("> ");
                    len = 0;
                    fflush(stdout);
                } else {
                    int word_start = len - 1;
                    while (word_start >= 0 && line[word_start] != ' ') {
                        word_start--;
                    }
                    word_start++;

                    if (word_start > 0) {
                        int word_len = len - word_start;
                        char word_buf[MAX_LINE + 1];
                        memcpy(word_buf, &line[word_start], word_len);

                        for (int k = 0; k < word_len; k++) {
                            erase_one_char();
                        }
                        putchar('\n');
                        printf("> ");

                        for (int k = 0; k < word_len; k++) {
                            putchar(word_buf[k]);
                            line[k] = word_buf[k];
                        }
                        line[word_len] = c;
                        putchar(c);
                        len = word_len + 1;
                        fflush(stdout);
                    } else {
                        putchar('\n');
                        printf("> ");
                        line[0] = c;
                        len = 1;
                        putchar(c);
                        fflush(stdout);
                    }
                }
            }
            continue;
        }

        // 7. Все непечатные символы
        ring_bell();
    }

    printf("\n[Редактор завершил работу]\n");
    return 0;
}
