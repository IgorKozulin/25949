#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    long offset;  
    int length;
} LineInfo;

static char *global_map = NULL;   
static size_t global_size = 0;

static void alarm_handler(int sig)
{
    static const char msg[] = "\nTime is up.\n";

    (void)sig;
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    write(STDOUT_FILENO, global_map, global_size);
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
    long line_start = 0;
    struct stat st;
    LineInfo *table = NULL;
    void *map;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }
    if (st.st_size == 0) {                   
        printf("File is empty.\n");
        close(fd);
        return 0;
    }

    map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    close(fd);                              
    global_map = map;
    global_size = st.st_size;

    for (i = 0; i < (int)global_size; i++) {
        if (global_map[i] == '\n') {
            if (add_line(&table, &num_lines, &capacity, line_start, (int)(i + 1 - line_start)) != 0) {
                fprintf(stderr, "Error: out of memory\n");
                munmap(map, global_size);
                return 1;
            }
            line_start = i + 1;
        }
    }
    if (line_start < global_size) {    
        if (add_line(&table, &num_lines, &capacity, line_start, (int)(global_size - line_start)) != 0) {
            fprintf(stderr, "Error: out of memory\n");
            munmap(map, global_size);
            return 1;
        }
    }

    for (i = 0; i < num_lines; i++)
        printf("Line %d: Offset = %ld, Length = %d\n", i + 1, table[i].offset, table[i].length);

    signal(SIGALRM, alarm_handler);

    while (1) {
        printf("Enter line number: ");
        fflush(stdout);

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

        fwrite(global_map + table[n - 1].offset, 1, table[n - 1].length, stdout);
        if (global_map[table[n - 1].offset + table[n - 1].length - 1] != '\n')
            printf("\n");        
    }

    free(table);
    munmap(map, global_size);
    return 0;
}
