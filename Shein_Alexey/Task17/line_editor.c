#define _XOPEN_SOURCE 600
#ifdef __sun
#define __EXTENSIONS__ 1
#endif

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define WIDTH 40
#define CTRL_D 4
#define CTRL_P 16
#define CTRL_W 23
#define ESC 27

struct Row {
    size_t length;
    char text[WIDTH];
};

struct Cursor {
    size_t row;
    size_t column;
    size_t top;
    int escape;
};

static struct termios original;
static struct Row *history;
static size_t history_count;
static size_t history_capacity;
static int browsing;

static void leave_browse(void)
{
    if (browsing) {
        fputs("\033[?1049l", stdout);
        browsing = 0;
    }
}

static void cleanup(void)
{
    int result;

    leave_browse();
    fflush(stdout);
    free(history);

    do {
        result = tcsetattr(STDIN_FILENO, TCSANOW, &original);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        perror("restore terminal");
    }
}

static int setup_terminal(void)
{
    struct termios mode;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "Run this program in a terminal.\n");
        return -1;
    }

    if (tcgetattr(STDIN_FILENO, &original) == -1) {
        perror("tcgetattr");
        return -1;
    }

    if (atexit(cleanup) != 0) {
        fprintf(stderr, "Cannot register cleanup.\n");
        return -1;
    }

    mode = original;
    mode.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    mode.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR | IGNCR);
    mode.c_oflag &= ~OPOST;
    mode.c_cc[VMIN] = 1;
    mode.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &mode) == -1) {
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

static int save_row(const char *text, size_t length)
{
    if (history_count == history_capacity) {
        size_t capacity;
        struct Row *new_history;

        if (history_capacity > SIZE_MAX / sizeof(*history) / 2) {
            errno = ENOMEM;
            return -1;
        }

        capacity = history_capacity ? history_capacity * 2 : 16;
        new_history = realloc(history, capacity * sizeof(*history));

        if (new_history == NULL) {
            return -1;
        }

        history = new_history;
        history_capacity = capacity;
    }

    history[history_count].length = length;
    memcpy(history[history_count].text, text, length);
    ++history_count;
    return 0;
}

static const struct Row *get_row(const struct Row *current, size_t index)
{
    return index == history_count ? current : &history[index];
}

static void redraw(const char *text, size_t length)
{
    fputs("\r\033[K", stdout);
    fwrite(text, 1, length, stdout);
}

static size_t erase_word(const char *text, size_t length)
{
    while (length > 0 && text[length - 1] == ' ') {
        --length;
    }

    while (length > 0 && text[length - 1] != ' ') {
        --length;
    }

    return length;
}

static int append_char(struct Row *line, unsigned char c)
{
    if (line->length == WIDTH) {
        size_t split = line->length;

        if (c != ' ') {
            while (split > 0 && line->text[split - 1] != ' ') {
                --split;
            }
        }

        if (split == 0) {
            split = line->length;
        }

        if (save_row(line->text, split) == -1) {
            return -1;
        }

        redraw(line->text, split);
        fputs("\r\n", stdout);
        line->length -= split;
        memmove(line->text, line->text + split, line->length);
    }

    line->text[line->length++] = (char)c;
    redraw(line->text, line->length);
    return 0;
}

static int is_edit_key(unsigned char c, int index, long disabled)
{
    cc_t key = original.c_cc[index];

    return (disabled < 0 || key != (cc_t)disabled) && c == key;
}

static void show_history(const struct Row *current, struct Cursor *cursor)
{
    struct winsize window;
    size_t height = 24;
    size_t i;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0 &&
        window.ws_row > 0) {
        height = window.ws_row;
    }

    if (cursor->row < cursor->top) {
        cursor->top = cursor->row;
    } else if (cursor->row - cursor->top >= height) {
        cursor->top = cursor->row - height + 1;
    }

    fputs("\033[2J", stdout);

    for (i = 0; i < height && i <= history_count - cursor->top; ++i) {
        const struct Row *row = get_row(current, cursor->top + i);

        printf("\033[%zu;1H", i + 1);
        fwrite(row->text, 1, row->length, stdout);
    }

    printf("\033[%zu;%zuH",
           cursor->row - cursor->top + 1, cursor->column + 1);
}

static int arrow_key(struct Cursor *cursor, unsigned char c)
{
    if (c == ESC) {
        cursor->escape = 1;
        return 0;
    }

    if (cursor->escape == 1) {
        cursor->escape = (c == '[' || c == 'O') ? 2 : 0;
        return 0;
    }

    if (cursor->escape == 2 && c >= '@' && c <= '~') {
        cursor->escape = 0;
        return c >= 'A' && c <= 'D' ? c : 0;
    }

    return 0;
}

static void move_cursor(const struct Row *current, struct Cursor *cursor,
                        int arrow)
{
    size_t length = get_row(current, cursor->row)->length;

    switch (arrow) {
    case 'A':
        if (cursor->row > 0) {
            --cursor->row;
        }
        break;
    case 'B':
        if (cursor->row < history_count) {
            ++cursor->row;
        }
        break;
    case 'C':
        if (cursor->column < length) {
            ++cursor->column;
        }
        break;
    case 'D':
        if (cursor->column > 0) {
            --cursor->column;
        }
        break;
    }

    length = get_row(current, cursor->row)->length;
    if (cursor->column > length) {
        cursor->column = length;
    }
}

int main(void)
{
    struct Row line = {0};
    struct Cursor cursor = {0};
    long disabled;

    if (setup_terminal() == -1) {
        return EXIT_FAILURE;
    }

    disabled = fpathconf(STDIN_FILENO, _PC_VDISABLE);

    for (;;) {
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return EXIT_FAILURE;
        }

        if (n == 0) {
            break;
        }

        if (c == CTRL_P) {
            if (browsing) {
                leave_browse();
                redraw(line.text, line.length);
            } else {
                browsing = 1;
                cursor.row = history_count;
                cursor.column = line.length;
                cursor.top = 0;
                cursor.escape = 0;

                fputs("\033[?1049h", stdout);
                show_history(&line, &cursor);
            }
        } else if (browsing) {
            int arrow = arrow_key(&cursor, c);

            if (arrow != 0) {
                move_cursor(&line, &cursor, arrow);
                show_history(&line, &cursor);
            }
        } else if (c == CTRL_D) {
            if (line.length == 0) {
                break;
            }
            putchar('\a');
        } else if (is_edit_key(c, VERASE, disabled)) {
            if (line.length > 0) {
                --line.length;
                redraw(line.text, line.length);
            }
        } else if (is_edit_key(c, VKILL, disabled)) {
            line.length = 0;
            redraw(line.text, line.length);
        } else if (c == CTRL_W) {
            line.length = erase_word(line.text, line.length);
            redraw(line.text, line.length);
        } else if (c == '\r' || c == '\n') {
            if (save_row(line.text, line.length) == -1) {
                perror("save row");
                return EXIT_FAILURE;
            }
            fputs("\r\n", stdout);
            line.length = 0;
        } else if (isprint(c)) {
            if (append_char(&line, c) == -1) {
                perror("save row");
                return EXIT_FAILURE;
            }
        } else {
            putchar('\a');
        }

        if (fflush(stdout) == EOF || ferror(stdout)) {
            perror("stdout");
            return EXIT_FAILURE;
        }
    }

    leave_browse();
    fputs("\r\nExiting...\r\n", stdout);

    if (fflush(stdout) == EOF || ferror(stdout)) {
        perror("stdout");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
