#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#define WIDTH 40

static struct termios saved;
static char line[WIDTH];   
static int len = 0;

static void out(const char *s, int n)
{
    write(STDOUT_FILENO, s, n);
}

static void erase(int n)
{
    for (; n > 0 && len > 0; n--, len--)
        out("\b \b", 3);
}

static void restore(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
}

int main(void)
{
    struct termios t;
    unsigned char c;

    if (tcgetattr(STDIN_FILENO, &saved) < 0) {
        perror("tcgetattr");
        return 1;
    }
    atexit(restore);

    t = saved;
    t.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 4 && len == 0) {                   
            break;
        } else if (c == saved.c_cc[VERASE]) {
            erase(1);
        } else if (c == saved.c_cc[VKILL]) {
            erase(len);
        } else if (c == 23) {                     
            int i = len;
            while (i > 0 && line[i - 1] == ' ')
                i--;
            while (i > 0 && line[i - 1] != ' ')
                i--;
            erase(len - i);
        } else if (c == '\n') {
            out("\n", 1);
            len = 0;
        } else if (!isprint(c)) {
            out("\a", 1);                              
        } else {
            if (len == WIDTH) {
                int s = len;
                while (s > 0 && line[s - 1] != ' ')
                    s--;
                int w = (s == 0 || c == ' ') ? 0 : len - s;

                erase(w);
                out("\n", 1);
                memmove(line, line + s, w);
                len = w;
                out(line, len);

                if (c == ' ')
                    continue;
            }
            line[len++] = c;
            out((char *)&c, 1);
        }
    }

    out("Exiting...\n", 11);
    return 0;
}
