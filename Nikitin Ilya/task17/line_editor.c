#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define MAX_LINE 40
#define BUFFER_SIZE 4096

static struct termios old_term;

static int previous_lines = 1;

void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
}

void enable_raw_mode(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &old_term);
    atexit(disable_raw_mode);
    raw = old_term;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void clear_previous_output(void)
{
    int i;
    printf("\r");
    for (i = 1; i < previous_lines; i++)
    {
        printf("\033[A");
    }
    for (i = 0; i < previous_lines; i++)
    {
        printf("\033[2K");

        if (i < previous_lines - 1)
        {
            printf("\033[B");
        }
    }
    printf("\r");

    for (i = 1; i < previous_lines; i++)
    {
        printf("\033[A");
    }
}

void redraw(const char *buffer, int len)
{
    int i = 0;
    int col = 2;
    int lines = 1;

    clear_previous_output();

    printf("> ");

    while (i < len)
    {
        int start;
        int word_len;
        int j;

        if (buffer[i] == ' ')
        {
            if (col < MAX_LINE)
            {
                putchar(' ');
                col++;
            }

            i++;
            continue;
        }

        start = i;

        while (i < len && buffer[i] != ' ')
        {
            i++;
        }

        word_len = i - start;

        if (col > 2 && col + word_len > MAX_LINE)
        {
            putchar('\n');

            col = 0;
            lines++;
        }

        for (j = 0; j < word_len; j++)
        {
            if (col == MAX_LINE)
            {
                putchar('\n');

                col = 0;
                lines++;
            }

            putchar(buffer[start + j]);
            col++;
        }
    }

    previous_lines = lines;

    fflush(stdout);
}

int delete_word(char *buffer, int len)
{
    /* Удаляем пробелы в конце */
    while (len > 0 && buffer[len - 1] == ' ')
    {
        len--;
    }

    /* Удаляем последнее слово */
    while (len > 0 && buffer[len - 1] != ' ')
    {
        len--;
    }

    /* Удаляем пробелы перед словом */
    while (len > 0 && buffer[len - 1] == ' ')
    {
        len--;
    }

    return len;
}

int main(void)
{
    char buffer[BUFFER_SIZE];
    int len = 0;

    struct termios original;

    tcgetattr(STDIN_FILENO, &original);

    enable_raw_mode();

    printf("> ");
    fflush(stdout);

    while (1)
    {
        char c;

        if (read(STDIN_FILENO, &c, 1) != 1)
        {
            break;
        }

        /*
         * Ctrl+D.
         */
        if (c == 4)
        {
            if (len == 0)
            {
                printf("\nExiting...\n");
                break;
            }

            putchar('\a');
            fflush(stdout);
            continue;
        }

        /*
         * ERASE ( Backspace).
         */
        if (c == original.c_cc[VERASE])
        {
            if (len > 0)
            {
                len--;
                buffer[len] = '\0';
                redraw(buffer, len);
            }
            else
            {
                putchar('\a');
                fflush(stdout);
            }
            continue;
        }

        /*
         * KILL (Ctrl+U).
         */
        if (c == original.c_cc[VKILL])
        {
            if (len > 0)
            {
                len = 0;
                buffer[0] = '\0';
                redraw(buffer, len);
            }
            continue;
        }

        /*
         * Ctrl+W.
         */
        if (c == 23)
        {
            if (len > 0)
            {
                len = delete_word(buffer, len);

                buffer[len] = '\0';

                redraw(buffer, len);
            }

            continue;
        }

        /*
         * Enter.
         */
        if (c == '\n' || c == '\r')
        {
            putchar('\n');
            len = 0;
            buffer[0] = '\0';
            previous_lines = 1;
            printf("> ");
            fflush(stdout);
            continue;
        }

        /*
         * Печатаемый символ.
         */
        if (isprint((unsigned char)c))
        {
            if (len < BUFFER_SIZE - 1)
            {
                buffer[len++] = c;
                buffer[len] = '\0';
                redraw(buffer, len);
            }
            else
            {
                putchar('\a');
                fflush(stdout);
            }
            continue;
        }

        /*
         * Любой другой непечатаемый символ.
         */
        putchar('\a');
        fflush(stdout);
    }

    return 0;
}