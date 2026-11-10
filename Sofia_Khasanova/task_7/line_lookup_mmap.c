#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

static void *g_map = NULL;
static size_t g_size = 0;

typedef struct {
    long offset;
    size_t length;
} LineInfo;

void alarm_handler(int signo)
{
    (void)signo;

    const char msg[] = "[TIMEOUT] Time is up! Printing full file content...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    if (g_map != NULL && g_size > 0) {
        const char *p = (const char *)g_map;
        size_t left = g_size;

        while (left > 0) {
            ssize_t w = write(STDOUT_FILENO, p, left);
            if (w < 0) _exit(EXIT_FAILURE);
            p += w;
            left -= (size_t)w;
        }
    }

    _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    int file = open(argv[1], O_RDONLY);
    if (file == -1) {
        perror("open");
        return 1;
    }

    struct stat st;
    if (fstat(file, &st) == -1) {
        perror("fstat");
        close(file);
        return 1;
    }

    if (st.st_size == 0) {
        printf("File is empty.\n");
        close(file);
        return 0;
    }

    size_t file_size = (size_t)st.st_size;

    void *m_map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, file, 0);
    if (m_map == MAP_FAILED) {
        perror("mmap");
        close(file);
        return 1;
    }

    close(file);

    g_map = m_map;
    g_size = file_size;

    signal(SIGALRM, alarm_handler);

    LineInfo *table = malloc(sizeof(*table));
    if (table == NULL) {
        perror("malloc");
        munmap(m_map, file_size);
        return 1;
    }

    size_t capacity = 1;
    size_t num_lines = 0;
    size_t line_start = 0;
    char *data = (char *)m_map;

    for (size_t i = 0; i < file_size; i++) {
        if (data[i] == '\n') {
            if (num_lines == capacity) {
                capacity *= 2;
                LineInfo *tmp = realloc(table, capacity * sizeof(*tmp));
                if (tmp == NULL) {
                    perror("realloc");
                    free(table);
                    munmap(m_map, file_size);
                    return 1;
                }
                table = tmp;
            }

            table[num_lines].offset = (long)line_start;
            table[num_lines].length = i - line_start;
            num_lines++;
            line_start = i + 1;
        }
    }

    if (line_start < file_size) {
        if (num_lines == capacity) {
            capacity *= 2;
            LineInfo *tmp = realloc(table, capacity * sizeof(*tmp));
            if (tmp == NULL) {
                perror("realloc");
                free(table);
                munmap(m_map, file_size);
                return 1;
            }
            table = tmp;
        }

        table[num_lines].offset = (long)line_start;
        table[num_lines].length = file_size - line_start;
        num_lines++;
    }

    for (size_t i = 0; i < num_lines; i++) {
        printf("line %zu: offset = %ld, length = %zu\n",
               i + 1, table[i].offset, table[i].length+1);
    }

    int num_line;

    while (1) {
        printf("У вас есть 5 секунд. Введите номер строки от 1 до %zu (0 - выход): ",
               num_lines);
        fflush(stdout);

        alarm(5);
        int rc = scanf("%d", &num_line);
        alarm(0);

        if (rc == EOF) {
            break;
        }

        if (rc != 1) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            printf("Invalid input\n");
            continue;
        }

        if (num_line == 0) break;

        if (num_line < 1 || (size_t)num_line > num_lines) {
            printf("Недопустимый номер\n");
            continue;
        }

        LineInfo li = table[num_line - 1];
        fwrite(data + li.offset, 1, li.length, stdout);
        putchar('\n');
    }

    free(table);
    munmap(m_map, file_size);

    return 0;
}
