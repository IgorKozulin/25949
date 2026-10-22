#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

#define WIDTH 40
#define CTRL_D 4
#define CTRL_W 23

static struct termios saved;
static int terminal_changed = 0;

static void restore_terminal(void)
{
    if (terminal_changed) {
        int result;

        do {
            result = tcsetattr(STDIN_FILENO, TCSANOW, &saved);
        } while (result == -1 && errno == EINTR);

        if (result == -1) {
            perror("restore terminal");
        } else {
            terminal_changed = 0;
        }
    }
}

static int setup_terminal(void)
{
    struct termios mode;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "Run this program in an interactive terminal\n");
        return -1;
    }

    if (tcgetattr(STDIN_FILENO, &saved) == -1) {
        perror("tcgetattr");
        return -1;
    }

    if (atexit(restore_terminal) != 0) {
        fprintf(stderr, "Cannot register terminal restoration\n");
        return -1;
    }

    mode = saved;

    mode.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    mode.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR | IGNCR);
    mode.c_oflag &= ~OPOST;

    mode.c_cc[VMIN] = 1;
    mode.c_cc[VTIME] = 0;

    /*
     * Set this before tcsetattr so restoration is attempted
     * even if applying the new settings fails.
     */
    terminal_changed = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &mode) == -1) {
        int error = errno;

        restore_terminal();
        errno = error;
        perror("tcsetattr");
        return -1;
    }

    return 0;
}

static void erase_chars(size_t count)
{
    while (count > 0) {
        /*
         * Move left, overwrite the character with a space,
         * then move left again.
         */
        fputs("\b \b", stdout);
        --count;
    }
}

static int is_control(unsigned char c, int index)
{
    return saved.c_cc[index] != (cc_t)_POSIX_VDISABLE &&
           c == saved.c_cc[index];
}

static void append_char(char *line, size_t *length,
                        unsigned char c)
{
    if (*length == WIDTH) {
        size_t start = *length;

        if (c != ' ') {
            while (start > 0 && line[start - 1] != ' ') {
                --start;
            }
        }

        if (c != ' ' && start > 0 && start < *length) {
            size_t word_length = *length - start;

            /* Remove the unfinished word from the old row. */
            erase_chars(word_length);
            fputs("\r\n", stdout);

            memmove(line, line + start, word_length);
            *length = word_length;

            fwrite(line, 1, *length, stdout);
        } else {
            /*
             * A full row, or a word longer than WIDTH:
             * continue on the next row.
             */
            fputs("\r\n", stdout);
            *length = 0;
        }
    }

    line[*length] = (char)c;
    ++*length;
    putchar(c);
}

int main(void)
{
    char line[WIDTH];
    size_t length = 0;
    int status = EXIT_SUCCESS;

    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "Cannot disable stdout buffering\n");
        return EXIT_FAILURE;
    }

    puts("Line editor: ERASE, KILL, Ctrl+W; Ctrl+D on an empty row exits.");
    puts("Use English letters, digits and spaces. Width: 40.");

    if (setup_terminal() == -1) {
        return EXIT_FAILURE;
    }

    while (1) {
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            restore_terminal();
            perror("read");
            status = EXIT_FAILURE;
            break;
        }

        if (n == 0) {
            break;
        }

        if (c == CTRL_D) {
            if (length == 0) {
                fputs("\r\nExiting...\r\n", stdout);
                break;
            }

            putchar('\a');
        } else if (is_control(c, VERASE)) {
            if (length > 0) {
                --length;
                erase_chars(1);
            } else {
                putchar('\a');
            }
        } else if (is_control(c, VKILL)) {
            erase_chars(length);
            length = 0;
        } else if (c == CTRL_W) {
            size_t old_length = length;

            while (length > 0 && line[length - 1] == ' ') {
                --length;
            }

            while (length > 0 && line[length - 1] != ' ') {
                --length;
            }

            erase_chars(old_length - length);
        } else if (c == '\r' || c == '\n') {
            fputs("\r\n", stdout);
            length = 0;
        } else if (isprint((unsigned char)c)) {
            append_char(line, &length, c);
        } else {
            putchar('\a');
        }

        if (ferror(stdout)) {
            status = EXIT_FAILURE;
            break;
        }
    }

    restore_terminal();
    return status;
}
