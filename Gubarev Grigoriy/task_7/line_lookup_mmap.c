#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TIMEOUT_SECONDS 5

typedef struct {
    size_t offset;
    size_t length;
} LineInfo;

/* The whole file mapped into memory; global so the signal handler can reach it. */
static const char *file_map = NULL;
static size_t file_size = 0;

static void write_all(int fd, const char *data, size_t length)
{
    while (length > 0) {
        ssize_t written = write(fd, data, length);
        if (written == -1) {
            if (errno == EINTR)
                continue;
            return;
        }
        data += written;
        length -= (size_t)written;
    }
}

/* Runs when alarm() expires: the file is already in memory, so just write it out. */
static void alarm_handler(int signal_number)
{
    static const char message[] = "\n[TIMEOUT] Time is up! Printing full file content...\n";

    (void)signal_number;
    write_all(STDOUT_FILENO, message, sizeof(message) - 1);
    write_all(STDOUT_FILENO, file_map, file_size);
    _exit(EXIT_SUCCESS);
}

static int add_line(LineInfo **table, size_t *count, size_t *capacity,
                    size_t offset, size_t length)
{
    if (*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 16 : *capacity * 2;
        LineInfo *grown;
        if (new_capacity < *capacity || new_capacity > SIZE_MAX / sizeof(**table)) {
            fprintf(stderr, "Too many lines in file\n");
            return -1;
        }
        grown = realloc(*table, new_capacity * sizeof(**table));
        if (grown == NULL) {
            perror("realloc");
            return -1;
        }
        *table = grown;
        *capacity = new_capacity;
    }
    (*table)[*count].offset = offset;
    (*table)[*count].length = length;
    ++*count;
    return 0;
}

/* The file is an ordinary byte array now: find each '\n' with a plain loop. */
static int build_table(LineInfo **table, size_t *count)
{
    size_t capacity = 0;
    size_t line_start = 0;
    size_t i;

    for (i = 0; i < file_size; ++i) {
        if (file_map[i] == '\n') {
            if (add_line(table, count, &capacity, line_start, i - line_start) == -1)
                return -1;
            line_start = i + 1;
        }
    }
    if (line_start < file_size &&
        add_line(table, count, &capacity, line_start, file_size - line_start) == -1)
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    struct stat info;
    void *mapped;
    LineInfo *table = NULL;
    size_t line_count = 0;
    char input[128];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <text-file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }
    if (fstat(fd, &info) == -1) {
        perror("fstat");
        close(fd);
        return EXIT_FAILURE;
    }
    if (info.st_size == 0) {
        puts("File is empty.");
        close(fd);
        return EXIT_SUCCESS;
    }
    if ((unsigned long long)info.st_size > (unsigned long long)SIZE_MAX) {
        fprintf(stderr, "File is too large to map\n");
        close(fd);
        return EXIT_FAILURE;
    }
    file_size = (size_t)info.st_size;
    mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return EXIT_FAILURE;
    }
    file_map = mapped;
    /* The mapping stays valid after the descriptor is closed. */
    if (close(fd) == -1)
        perror("close");

    if (build_table(&table, &line_count) == -1) {
        free(table);
        munmap(mapped, file_size);
        return EXIT_FAILURE;
    }

    puts("--- Line table (line: offset, length) ---");
    for (size_t i = 0; i < line_count; ++i)
        printf("%zu: %zu, %zu\n", i + 1, table[i].offset, table[i].length);

    if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
        perror("signal");
        free(table);
        munmap(mapped, file_size);
        return EXIT_FAILURE;
    }

    for (;;) {
        char *end;
        long number;
        char *got;
        LineInfo line;

        printf("Line number (0 to quit, %d sec timeout): ", TIMEOUT_SECONDS);
        /* Flush before the timer: the handler uses write(), which bypasses stdio buffers. */
        fflush(stdout);
        alarm(TIMEOUT_SECONDS);
        got = fgets(input, sizeof(input), stdin);
        alarm(0);
        if (got == NULL)
            break;

        errno = 0;
        number = strtol(input, &end, 10);
        if (errno == ERANGE || end == input || (*end != '\n' && *end != '\0')) {
            puts("Enter an integer line number.");
            continue;
        }
        if (number == 0)
            break;
        if (number < 1 || (unsigned long)number > line_count) {
            puts("Line number is out of range.");
            continue;
        }
        line = table[number - 1];
        /* fwrite prints exactly length bytes straight from the mapping, no '\0' needed. */
        fwrite(file_map + line.offset, 1, line.length, stdout);
        putchar('\n');
    }
    free(table);
    if (munmap(mapped, file_size) == -1) {
        perror("munmap");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
