#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define MAX_BUF    4096
#define LINE_WIDTH 40

static struct termios orig_termios;
static int raw_enabled = 0;

static void disable_raw_mode(void)
{
    if (raw_enabled)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_enabled = 0;
    }
}

static void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    {
        perror("tcgetattr");
        exit(1);
    }
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        perror("tcsetattr");
        exit(1);
    }
    raw_enabled = 1;
}

static void redraw(const char *buf, int pos)
{
    printf("\r\033[K");

    int col = 0;
    int i = 0;

    while (i < pos)
    {
        if (isspace((unsigned char)buf[i]))
        {
            while (i < pos && isspace((unsigned char)buf[i]))
            {
                if (col >= LINE_WIDTH)
                {
                    printf("\n\033[K");
                    col = 0;
                }
                putchar(buf[i]);
                col++;
                i++;
            }
            continue;
        }

        int word_start = i;
        int word_end = i;
        while (word_end < pos && !isspace((unsigned char)buf[word_end]))
            word_end++;

        int word_len = word_end - word_start;

        if (col + word_len > LINE_WIDTH && col > 0)
        {
            printf("\n\033[K");
            col = 0;
        }

        for (int k = word_start; k < word_end; k++)
        {
            if (col >= LINE_WIDTH)
            {
                printf("\n\033[K");
                col = 0;
            }
            putchar(buf[k]);
            col++;
        }
        i = word_end;
    }

    fflush(stdout);
}

static int erase_word(const char *buf, int pos)
{
    if (pos <= 0) return 0;
    int p = pos;
    while (p > 0 && isspace((unsigned char)buf[p - 1]))
        p--;
    while (p > 0 && !isspace((unsigned char)buf[p - 1]))
        p--;
    return p;
}

int main(void)
{
    enable_raw_mode();

    cc_t erase_char = orig_termios.c_cc[VERASE];
    cc_t kill_char  = orig_termios.c_cc[VKILL];

    char buf[MAX_BUF];
    memset(buf, 0, sizeof(buf));
    int pos = 0;

    printf("> ");
    fflush(stdout);

    char c;
    while (1)
    {
        if (read(STDIN_FILENO, &c, 1) <= 0)
            continue;

        unsigned char uc = (unsigned char)c;

        if (uc == 4)
        {
            if (pos == 0)
            {
                printf("\r\033[K");
                printf("Exiting...\n");
                break;
            }
            continue;
        }

        if (uc == (unsigned char)erase_char || uc == 0x7F)
        {
            if (pos > 0)
            {
                pos--;
                buf[pos] = '\0';
                redraw(buf, pos);
            }
            else
            {
                putchar('\a');
                fflush(stdout);
            }
            continue;
        }

        if (uc == (unsigned char)kill_char)
        {
            if (pos > 0)
            {
                pos = 0;
                buf[0] = '\0';
                redraw(buf, pos);
            }
            else
            {
                putchar('\a');
                fflush(stdout);
            }
            continue;
        }

        if (uc == 23)
        {
            if (pos > 0)
            {
                pos = erase_word(buf, pos);
                buf[pos] = '\0';
                redraw(buf, pos);
            }
            else
            {
                putchar('\a');
                fflush(stdout);
            }
            continue;
        }

        if (uc == '\n' || uc == '\r')
        {
            putchar('\n');
            printf("> ");
            fflush(stdout);
            pos = 0;
            buf[0] = '\0';
            continue;
        }

        if (isprint(uc))
        {
            if (pos < MAX_BUF - 1)
            {
                buf[pos++] = (char)uc;
                buf[pos] = '\0';
                redraw(buf, pos);
            }
            continue;
        }

        putchar('\a');
        fflush(stdout);
    }

    disable_raw_mode();
    return 0;
}
