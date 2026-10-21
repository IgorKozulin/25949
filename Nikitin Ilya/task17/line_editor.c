#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define MAX_LINE 40
#define BUFFER_SIZE 4096

static struct termios old_term;

void disabled_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
}

void enable_raw_mode(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &old_term);
    atexit(disabled_raw_mode);
    raw = old_term;

    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void redraw(const char *buffer, int len)
{
    int i = 0;
    int col = 0;
    printf("\r\033[2K");

    while (i < len)
    {
        int start;
        int word_len;

        if (isspace((unsigned char)buffer[i]))
        {
            if (buffer[i] == ' ')
            {
                if (col < MAX_LINE)
                {
                    putchar(' ');
                    col++;
                }
            }
            i++;
            continue;
        }
        start = i;
        while (i < len && !isspace((unsigned char)buffer[i]))
        {
            i++;
        }
        word_len = i - start;
        if (col > 0 && col + word_len > MAX_LINE)
        {
            printf("\n");
            col = 0;
        }
        if (word_len > MAX_LINE)
        {
            int j;
            for (j = 0; j < word_len; j++)
            {
                if (col == MAX_LINE)
                {
                    printf("\n");
                    col = 0;
                }
                putchar(buffer[start + j]);
                col++;
            }
        }
        else
        {
            int j;
            for (j = 0; j < word_len; j++)
            {
                putchar(buffer[start + j]);
                col++;
            }
        }
    }
    fflush(stdout);
}

int delete_word(char *buffer, int len)
{
    while (len > 0 && buffer[len - 1] == ' ')
    {
        len--;
    }
    while (len > 0 && buffer[len - 1] != ' ')
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
        if (c == 4) //  Ctrl+D (код 4)
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
        if (c == original.c_cc[VERASE]) // ERASE (Backspace):
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
        if (c == original.c_cc[VKILL]) // KILL (Ctrl+U)
        {
            if (len > 0)
            {
                len = 0;
                buffer[0] = '\0';
                redraw(buffer, len);
            }
            continue;
        }
        if (c == 23) // Ctrl+W (код 23)
        {
            if (len > 0)
            {
                len = delete_word(buffer, len);
                buffer[len] = '\0';
                redraw(buffer, len);
            }
            continue;
        }
        if (c == '\n' || c == '\r') // Enter (\n или \r)
        {
            putchar('\n');
            len = 0;
            buffer[0] = '\0';
            printf("> ");
            fflush(stdout);
            continue;
        }

        if (isprint((unsigned char)c)) // символ печатаемый (isprint)
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
        putchar('\a');
        fflush(stdout);
    }
    return 0;
}