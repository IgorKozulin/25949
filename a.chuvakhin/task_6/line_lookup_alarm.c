#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

typedef struct {
    long offset;  
    int length;   
} LineInfo;

static int global_fd = -1;   

static void alarm_handler(int sig)
{
    static const char msg[] = "\nTime is up.\n";
    char buf[512];
    ssize_t got;

    (void)sig;
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    if (lseek(global_fd, 0, SEEK_SET) != -1) {
        while ((got = read(global_fd, buf, sizeof(buf))) > 0)
            write(STDOUT_FILENO, buf, got);
    }
    _exit(EXIT_SUCCESS);
}

static int add_line(LineInfo **table, int *count, int *capacity, long offset, int length)
{
    if (*count == *capacity) {
        int new_cap = (*capacity == 0) ? 16 : *capacity * 2;
        LineInfo *tmp = realloc(*table, new_cap * sizeof(LineInfo));
        if (tmp == NULL)
            return -1;
        *table = tmp;
        *capacity = new_cap;
    }
    (*table)[*count].offset = offset;
    (*table)[*count].length = length;
    (*count)++;
    return 0;
}

int main(int argc, char *argv[])
{
    int fd, num_lines = 0, capacity = 0, i, n, ok;
    long line_start = 0, pos;
    char c;
    LineInfo *table = NULL;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    global_fd = fd;

    while (read(fd, &c, 1) == 1) {
        pos = lseek(fd, 0L, SEEK_CUR);      
        if (c == '\n') {
            if (add_line(&table, &num_lines, &capacity, line_start, (int)(pos - line_start)) != 0) {
                fprintf(stderr, "Error: out of memory\n");
                return 1;
            }
            line_start = pos;                   
        }
    }
    pos = lseek(fd, 0L, SEEK_CUR);             
    if (pos > line_start) {                   
        if (add_line(&table, &num_lines, &capacity, line_start, (int)(pos - line_start)) != 0) {
            fprintf(stderr, "Error: out of memory\n");
            return 1;
        }
    }

    if (num_lines == 0) {
        printf("File is empty.\n");
        close(fd);
        return 0;
    }

    for (i = 0; i < num_lines; i++)
        printf("Line %d: Offset = %ld, Length = %d\n", i + 1, table[i].offset, table[i].length);

    signal(SIGALRM, alarm_handler);

    while (1) {
        printf("Enter line number: ");

        alarm(5);
        ok = scanf("%d", &n);
        alarm(0);                               

        if (ok != 1) {
            if (feof(stdin))
                break;
            printf("Invalid input, enter a number.\n");
            while ((i = getchar()) != '\n' && i != EOF);                           
            continue;
        }

        if (n == 0)
            break;
        if (n < 1 || n > num_lines) {
            printf("No such line (valid: 1..%d).\n", num_lines);
            continue;
        }

        {
            LineInfo *li = &table[n - 1];
            char *buf = malloc(li->length + 1);
            ssize_t got;

            if (buf == NULL) {
                fprintf(stderr, "Error: out of memory\n");
                break;
            }
            if (lseek(fd, li->offset, SEEK_SET) == -1) {
                perror("lseek");
                free(buf);
                break;
            }
            got = read(fd, buf, li->length);
            if (got < 0) {
                perror("read");
                free(buf);
                break;
            }
            buf[got] = '\0';
            printf("%s", buf);
            if (got > 0 && buf[got - 1] != '\n')
                printf("\n");             
            free(buf);
        }
    }

    close(fd);
    free(table);
    return 0;
}
