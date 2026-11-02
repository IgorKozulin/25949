
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#define MAX_LINE 4096
#define WIDTH 40

struct termios original_term;

int terminal_changed = 0;
int rendered_rows = 0;

void disable_raw_mode(void)
{
    if (terminal_changed) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_term);
        terminal_changed = 0;
    }
}

void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &original_term) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    struct termios raw = original_term;

    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | INPCK | ISTRIP | BRKINT);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    terminal_changed = 1;
    atexit(disable_raw_mode);
}

void redraw(const char *buffer, int len)
{
    if (rendered_rows > 0) {
        printf("\033[%dA", rendered_rows);
    }

    printf("\r\033[J");

    int col = 0;
    int rows = 0;
    int i = 0;

    while (i < len) {
        if (isspace((unsigned char)buffer[i])) {
            int j = i;

            while (j < len &&
                   isspace((unsigned char)buffer[j])) {
                j++;
            }

            int word_end = j;

            while (word_end < len &&
                   !isspace((unsigned char)buffer[word_end])) {
                word_end++;
            }

            int word_len = word_end - j;

            if (word_len > 0 &&
                col > 0 &&
                col + (j - i) + word_len > WIDTH) {
                putchar('\n');
                col = 0;
                rows++;
                i = j;
                continue;
            }

            while (i < j) {
                if (col >= WIDTH) {
                    putchar('\n');
                    col = 0;
                    rows++;
                }

                putchar(buffer[i]);
                col++;
                i++;
            }

            continue;
        }

        int j = i;

        while (j < len &&
               !isspace((unsigned char)buffer[j])) {
            j++;
        }

        int word_len = j - i;

        if (col > 0 && col + word_len > WIDTH) {
            putchar('\n');
            col = 0;
            rows++;
        }

        while (i < j) {
            if (col >= WIDTH) {
                putchar('\n');
                col = 0;
                rows++;
            }

            putchar(buffer[i]);
            col++;
            i++;
        }
    }

    fflush(stdout);
    rendered_rows = rows;
}

int main(void)
{
    char buffer[MAX_LINE];
    int len = 0;
    char c;

    memset(buffer, 0, sizeof(buffer));

    enable_raw_mode();

    unsigned char erase_char = original_term.c_cc[VERASE];
    unsigned char kill_char = original_term.c_cc[VKILL];

    while (1) {
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("read");
            break;
        }

        if (n == 0) {
            break;
        }

        unsigned char ch = (unsigned char)c;

        /* Ctrl+D: выход, только если строка пустая */
        if (ch == 4) {
            if (len == 0) {
                printf("\r\033[JExiting...\n");
                fflush(stdout);
                break;
            }

            putchar('\a');
            fflush(stdout);
        }

        /* Backspace или клавиша стирания терминала */
        else if (ch == erase_char ||
                 ch == 127 ||
                 ch == '\b') {
            if (len > 0) {
                len--;
                buffer[len] = '\0';
                redraw(buffer, len);
            }
        }

        /* Ctrl+U или настроенная клавиша VKILL */
        else if (ch == 21 || ch == kill_char) {
            len = 0;
            buffer[0] = '\0';
            redraw(buffer, len);
        }

        /* Ctrl+W: удалить слово и пробелы перед ним */
        else if (ch == 23) {
            while (len > 0 &&
                   isspace((unsigned char)buffer[len - 1])) {
                len--;
            }

            while (len > 0 &&
                   !isspace((unsigned char)buffer[len - 1])) {
                len--;
            }

            while (len > 0 &&
                   isspace((unsigned char)buffer[len - 1])) {
                len--;
            }

            buffer[len] = '\0';
            redraw(buffer, len);
        }

        /* Enter: завершить текущую строку и начать новую */
        else if (c == '\r' || c == '\n') {
            putchar('\n');
            fflush(stdout);

            len = 0;
            buffer[0] = '\0';
            rendered_rows = 0;
        }

        /* Обычный печатный символ */
        else if (isprint(ch)) {
            if (len < MAX_LINE - 1) {
                buffer[len++] = c;
                buffer[len] = '\0';
                redraw(buffer, len);
            } else {
                putchar('\a');
                fflush(stdout);
            }
        }

        /* Остальные управляющие символы */
        else {
            putchar('\a');
            fflush(stdout);
        }
    }

    return 0;
}
