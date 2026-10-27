#include <stdio.h>        // printf, scanf, fflush, fwrite
#include <stdlib.h>       // malloc, realloc, free, exit
#include <unistd.h>       // close, alarm, _exit, write
#include <fcntl.h>        // open, O_RDONLY
#include <signal.h>       // signal, SIGALRM
#include <sys/mman.h>     // mmap, munmap, MAP_FAILED
#include <sys/stat.h>     // fstat, struct stat
#include <string.h>       // strlen

typedef struct {
    long offset;
    int length;
} LineInfo;

static char *g_map = NULL;
static size_t g_size = 0;

static void alarm_handler(int sig) {
    (void)sig;

    const char *msg = "\n[TIMEOUT] Time is up! Printing full file content...\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    write(STDOUT_FILENO, g_map, g_size);

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

    struct stat st;
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

    size_t size = (size_t)st.st_size;

    char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    close(fd);

    g_map = map;
    g_size = size;

    int capacity = 16;
    int num_lines = 0;
    LineInfo *table = malloc(capacity * sizeof(LineInfo));

    if (table == NULL) { 
        perror("malloc"); 
        munmap(map, size); 
        return 1; 
    }

    long offset = 0;
    int length = 0;

    for (size_t i = 0; i < size; i++) {
        if (map[i] == '\n') {
            if (num_lines >= capacity) {
                capacity *= 2;
                LineInfo *new_table = realloc(table, capacity * sizeof(LineInfo));
                if (new_table == NULL) { 
                    perror("realloc"); 
                    free(table); 
                    munmap(map, size); 
                    return 1; 
                }
                table = new_table;
            }
            table[num_lines].offset = offset;
            table[num_lines].length = length;
            num_lines++;
            offset = (long)(i + 1);
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
                munmap(map, size); 
                return 1; 
            }
            table = new_table;
        }
        table[num_lines].offset = offset;
        table[num_lines].length = length;
        num_lines++;
    }

    printf("--- Debug: Line Table ---\n");
    for (int i = 0; i < num_lines; i++) {
        printf("Line %d: Offset = %ld, Length = %d\n",
               i + 1, table[i].offset, table[i].length);
    }
    printf("-------------------------\n");

    if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
        perror("signal");
        free(table);
        munmap(map, size);
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

        if (n == 0) break;
        if (n < 1 || n > num_lines) {
            printf("Invalid line number (1..%d)\n", num_lines);
            continue;
        }

        int idx = n - 1;

        fwrite(map + table[idx].offset, 1, table[idx].length, stdout);
        printf("\n");
    }

    free(table);
    munmap(map, size);
    return 0;
}