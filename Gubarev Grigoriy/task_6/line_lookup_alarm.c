#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TIMEOUT_SECONDS 5

typedef struct {
    off_t offset;
    off_t length;
} LineInfo;

/* The signal handler has a fixed signature, so it reaches the file through a global. */
static int file_fd = -1;

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

/* Runs when alarm() expires: print the whole file with read/write only, then _exit. */
static void alarm_handler(int signal_number)
{
    static const char message[] = "\n[TIMEOUT] Time is up! Printing full file content...\n";
    char buffer[4096];
    ssize_t bytes;

    (void)signal_number;
    write_all(STDOUT_FILENO, message, sizeof(message) - 1);
    if (lseek(file_fd, 0, SEEK_SET) != (off_t)-1) {
        while ((bytes = read(file_fd, buffer, sizeof(buffer))) != 0) {
            if (bytes == -1) {
                if (errno == EINTR)
                    continue;
                break;
            }
            write_all(STDOUT_FILENO, buffer, (size_t)bytes);
        }
    }
    _exit(EXIT_SUCCESS);
}

static int add_line(LineInfo **table, size_t *count, size_t *capacity,
                    off_t offset, off_t length)
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

static int build_table(int fd, LineInfo **table, size_t *count)
{
    char buffer[4096];
    size_t capacity = 0;
    off_t line_start = 0;
    off_t line_length = 0;
    ssize_t bytes;

    while ((bytes = read(fd, buffer, sizeof(buffer))) != 0) {
        ssize_t i;
        if (bytes == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            return -1;
        }
        for (i = 0; i < bytes; ++i) {
            if (buffer[i] == '\n') {
                off_t next = lseek(fd, 0, SEEK_CUR);
                if (next == (off_t)-1) {
                    perror("lseek");
                    return -1;
                }
                /* read() advanced by the whole block; use this byte's absolute position. */
                next = next - (off_t)bytes + i + 1;
                if (add_line(table, count, &capacity, line_start, line_length) == -1)
                    return -1;
                line_start = next;
                line_length = 0;
            } else {
                ++line_length;
            }
        }
    }
    if (line_length > 0 && add_line(table, count, &capacity, line_start, line_length) == -1)
        return -1;
    return 0;
}

static int read_exact(int fd, char *buffer, size_t length)
{
    size_t done = 0;
    while (done < length) {
        ssize_t bytes = read(fd, buffer + done, length - done);
        if (bytes == -1 && errno == EINTR)
            continue;
        if (bytes == -1) {
            perror("read line");
            return -1;
        }
        if (bytes == 0) {
            fprintf(stderr, "Unexpected end of file\n");
            return -1;
        }
        done += (size_t)bytes;
    }
    return 0;
}

static int print_line(int fd, LineInfo line)
{
    char *text;

    if ((unsigned long long)line.length >= (unsigned long long)SIZE_MAX) {
        fprintf(stderr, "Line is too long to display\n");
        return 0;
    }
    text = malloc((size_t)line.length + 1);
    if (text == NULL) {
        perror("malloc");
        return -1;
    }
    if (lseek(fd, line.offset, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        free(text);
        return -1;
    }
    if (read_exact(fd, text, (size_t)line.length) == -1) {
        free(text);
        return -1;
    }
    text[line.length] = '\0';
    printf("%s\n", text);
    free(text);
    return 0;
}

int main(int argc, char **argv)
{
    LineInfo *table = NULL;
    size_t line_count = 0;
    char input[128];
    int status = EXIT_SUCCESS;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <text-file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    file_fd = open(argv[1], O_RDONLY);
    if (file_fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }
    if (build_table(file_fd, &table, &line_count) == -1) {
        free(table);
        close(file_fd);
        return EXIT_FAILURE;
    }

    puts("--- Line table (line: offset, length) ---");
    for (size_t i = 0; i < line_count; ++i)
        printf("%zu: %lld, %lld\n", i + 1, (long long)table[i].offset,
               (long long)table[i].length);

    if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
        perror("signal");
        free(table);
        close(file_fd);
        return EXIT_FAILURE;
    }

    for (;;) {
        char *end;
        long number;
        char *got;

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
        if (print_line(file_fd, table[number - 1]) == -1) {
            status = EXIT_FAILURE;
            break;
        }
    }
    free(table);
    if (close(file_fd) == -1) {
        perror("close");
        return EXIT_FAILURE;
    }
    return status;
}
