#include <stdio.h>        // printf, scanf, fflush
#include <stdlib.h>       // malloc, free, exit
#include <unistd.h>       // read, write, alarm, _exit
#include <fcntl.h>        // open
#include <signal.h>       // signal, SIGALRM
#include <string.h>       // strlen

typedef struct {
    long offset;
    int length;
} LineInfo;

static int g_fd = -1;
static LineInfo *g_table = NULL;
static int g_num_lines = 0;

static void alarm_handler(int sig) {
    (void)sig;
    const char *msg = "\n[TIMEOUT] Time is up! Printing full file content...\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    if (lseek(g_fd, 0, SEEK_SET) == -1) {   
        perror("lseek");
        _exit(EXIT_FAILURE);
    }

    char buf[4096];
    ssize_t n;
    while ((n = read(g_fd, buf, sizeof(buf))) > 0) {
        write(STDOUT_FILENO, buf, n);
    }

    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    g_fd = fd;

    int capacity = 16;
    int num_lines = 0;
    LineInfo *table = malloc(capacity * sizeof(LineInfo));
    if (table == NULL) { 
        perror("malloc"); 
        close(fd); 
        return 1;
    }

    long offset = 0;
    int length = 0;
    char c;

    while (read(fd, &c, 1) == 1) {
        if (c == '\n') {
            if (num_lines >= capacity) {
                capacity *= 2;
                LineInfo *new_table = realloc(table, capacity * sizeof(LineInfo));
                if (new_table == NULL) { 
                    perror("realloc"); 
                    free(table); 
                    close(fd); 
                    return 1; 
                }
                table = new_table;
            }
            table[num_lines].offset = offset;
            table[num_lines].length = length;
            num_lines++;
            offset = lseek(fd, 0L, SEEK_CUR);
            length = 0;
        } 
        else {
            length++;
        }
    }

    if (length > 0) {
        if (num_lines >= capacity) {
            capacity++;
            LineInfo *new_table = realloc(table, capacity * sizeof(LineInfo));
            if (new_table == NULL) { 
                perror("realloc"); 
                free(table); 
                close(fd); 
                return 1; 
            }
            table = new_table;
        }
        table[num_lines].offset = offset;
        table[num_lines].length = length;
        num_lines++;
    }

    g_table = table;
    g_num_lines = num_lines;

    printf("--- Debug: Line Table ---\n");
    for (int i = 0; i < num_lines; i++) {
        printf("Line %d: Offset = %ld, Length = %d\n",
               i + 1, table[i].offset, table[i].length);
    }
    printf("-------------------------\n");

    if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
        perror("signal");
        free(table);
        close(fd);
        return 1;
    }

    int n;
    while (1) {
        printf("Enter line number (0 to quit, 5 sec timeout): ");
        fflush(stdout);

        alarm(5);

        if (scanf("%d", &n) != 1) {
            alarm(0);
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            printf("Invalid input\n");
            continue;
        }

        alarm(0);

        if (n == 0) {
            break;
        }

        if (n < 1 || n > num_lines) {
            printf("Invalid line number (1..%d)\n", num_lines);
            continue;
        }

        int idx = n - 1;

        if (lseek(fd, table[idx].offset, SEEK_SET) == -1) {
            perror("lseek");
            continue;
        }

        char *buf = malloc(table[idx].length + 1);
        if (buf == NULL) { 
            perror("malloc"); 
            break;
        }

        ssize_t got = read(fd, buf, table[idx].length);
        if (got == -1) { 
            perror("read"); 
            free(buf); 
            continue; 
        }

        buf[got] = '\0';
        printf("%s\n", buf);

        free(buf);
    }

    free(table);
    close(fd);
    return 0;
}