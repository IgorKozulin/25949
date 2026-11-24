#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define LINE_WIDTH 40
#define BUFFER_SIZE 4096
#define CTRL_D 4
#define CTRL_G 7
#define CTRL_W 23
#define BACKSPACE 8
#define DELETE 127

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0
#endif

/* The whole line as it must look on the screen, rows separated by '\n'. */
typedef struct {
    char text[2 * BUFFER_SIZE];
    size_t used;
    int end_row; /* where the cursor stands after the last character */
    int end_col;
} Layout;

static struct termios saved_termios;
static volatile sig_atomic_t raw_enabled = 0;
static int cursor_row = 0; /* screen row of the cursor, counted from the first row of the line */

static void restore_terminal(void)
{
    if (raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
        raw_enabled = 0;
    }
}

/* Ctrl-C and friends still kill us (ISIG stays on), but the terminal must be given back first. */
static void on_signal(int signal_number)
{
    if (raw_enabled)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios); /* async-signal-safe */
    raise(signal_number); /* SA_RESETHAND restored the default action, so this ends the process */
}

static void install_signal_handlers(void)
{
    static const int signals[] = { SIGINT, SIGQUIT, SIGTERM, SIGHUP };
    struct sigaction action;
    size_t i;

    memset(&action, 0, sizeof(action));
    action.sa_handler = on_signal;
    action.sa_flags = SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i)
        sigaction(signals[i], &action, NULL);
}

/* Echo and canonical mode off: the kernel stops editing the line, every key reaches read() at once. */
static int enable_raw_mode(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &saved_termios) == -1) {
        perror("tcgetattr");
        return -1;
    }
    raw = saved_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN); /* IEXTEN: the driver would swallow Ctrl-W itself */
    raw.c_iflag &= ~IXON;                     /* Ctrl-S / Ctrl-Q must reach us (and just beep) */
    raw.c_cc[VMIN] = 1;                       /* read() returns after exactly one byte */
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        return -1;
    }
    raw_enabled = 1;
    return 0;
}

static void put(Layout *layout, char c)
{
    if (layout->used < sizeof(layout->text) - 1)
        layout->text[layout->used++] = c;
}

/*
 * Word wrap. A row holds at most LINE_WIDTH columns. A word that would cross the
 * last column moves to the start of the next row as a whole; only a word longer
 * than a whole row is cut. A space typed in the last column just starts the next row.
 */
static void layout_line(const char *buffer, size_t length, Layout *layout)
{
    size_t i = 0;
    int row = 0;
    int col = 0;

    layout->used = 0;
    while (i < length) {
        size_t word = 0;
        size_t k;

        if (buffer[i] == ' ') {
            if (col == LINE_WIDTH) {
                put(layout, '\n');
                ++row;
                col = 0;
            } else {
                put(layout, ' ');
                ++col;
            }
            ++i;
            continue;
        }
        while (i + word < length && buffer[i + word] != ' ')
            ++word;
        if (col > 0 && col + (int)word > LINE_WIDTH) {
            put(layout, '\n');
            ++row;
            col = 0;
        }
        for (k = 0; k < word; ++k) {
            if (col == LINE_WIDTH) {
                put(layout, '\n');
                ++row;
                col = 0;
            }
            put(layout, buffer[i + k]);
            ++col;
        }
        i += word;
    }
    layout->text[layout->used] = '\0';
    layout->end_row = row;
    layout->end_col = col;
}

/* After any change: go to the first row of the line, wipe it, draw it again. */
static void redraw(const char *buffer, size_t length)
{
    Layout layout;

    layout_line(buffer, length, &layout);
    if (cursor_row > 0)
        printf("\033[%dA", cursor_row); /* cursor up to the first row */
    fputs("\r\033[J", stdout);          /* start of row, erase to the end of the screen */
    fputs(layout.text, stdout);
    fflush(stdout);
    cursor_row = layout.end_row;
}

static void beep(void)
{
    putchar(CTRL_G);
    fflush(stdout);
}

int main(void)
{
    char buffer[BUFFER_SIZE];
    size_t length = 0;
    unsigned char erase_char;
    unsigned char kill_char;

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "Standard input is not a terminal\n");
        return EXIT_FAILURE;
    }
    if (enable_raw_mode() == -1)
        return EXIT_FAILURE;
    atexit(restore_terminal);
    install_signal_handlers();

    /* The terminal's own ERASE and KILL characters (usually Backspace/DEL and Ctrl-U). */
    erase_char = saved_termios.c_cc[VERASE];
    kill_char = saved_termios.c_cc[VKILL];

    for (;;) {
        unsigned char c;
        ssize_t got = read(STDIN_FILENO, &c, 1);

        if (got == 0)
            break;
        if (got == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            return EXIT_FAILURE;
        }

        if (c == CTRL_D) {
            if (length == 0) { /* Ctrl-D on an empty line ends the program */
                puts("Exiting...");
                break;
            }
            beep();
        } else if (c == '\n' || c == '\r') {
            putchar('\n');
            fflush(stdout);
            length = 0;
            cursor_row = 0;
        } else if ((erase_char != _POSIX_VDISABLE && c == erase_char) || c == BACKSPACE || c == DELETE) {
            if (length > 0) {
                --length;
                redraw(buffer, length);
            }
        } else if (kill_char != _POSIX_VDISABLE && c == kill_char) {
            if (length > 0) {
                length = 0;
                redraw(buffer, length);
            }
        } else if (c == CTRL_W) {
            /* The spaces after the last word go together with the word. */
            while (length > 0 && buffer[length - 1] == ' ')
                --length;
            while (length > 0 && buffer[length - 1] != ' ')
                --length;
            redraw(buffer, length);
        } else if (c >= ' ' && c < DELETE) {
            if (length < BUFFER_SIZE - 1) {
                buffer[length++] = (char)c;
                redraw(buffer, length);
            } else {
                beep();
            }
        } else {
            beep(); /* any other non-printing byte */
        }
    }
    return EXIT_SUCCESS;
}
