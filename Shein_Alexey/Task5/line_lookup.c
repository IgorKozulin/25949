#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

struct Line {
    off_t offset;
    size_t length;
};

static int add_line(struct Line **table, size_t *count,
                    off_t offset, size_t length)
{
    struct Line *new_table;

    if (*count >= SIZE_MAX / sizeof(**table)) {
        fprintf(stderr, "Too many lines\n");
        return -1;
    }

    new_table = realloc(*table, (*count + 1) * sizeof(**table));
    if (new_table == NULL) {
        perror("realloc");
        return -1;
    }

    *table = new_table;
    (*table)[*count].offset = offset;
    (*table)[*count].length = length;
    ++*count;
    return 0;
}

static int build_table(int fd, struct Line **table, size_t *count)
{
    off_t start = 0;
    size_t length = 0;
    char c;
    ssize_t n;

    while (1) {
        n = read(fd, &c, 1);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            return -1;
        }

        if (n == 0) {
            break;
        }

        if (length == SIZE_MAX - 1) {
            fprintf(stderr, "Line is too long\n");
            return -1;
        }
        ++length;

        if (c == '\n') {
            if (add_line(table, count, start, length) == -1) {
                return -1;
            }

            start = lseek(fd, 0, SEEK_CUR);
            if (start == (off_t)-1) {
                perror("lseek");
                return -1;
            }

            length = 0;
        }
    }

    if (length > 0) {
        return add_line(table, count, start, length);
    }

    return 0;
}

static int print_line(int fd, const struct Line *line)
{
    char *buffer;
    size_t done = 0;
    int status = 0;

    if (lseek(fd, line->offset, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        return -1;
    }

    buffer = malloc(line->length + 1);
    if (buffer == NULL) {
        perror("malloc");
        return -1;
    }

    while (done < line->length) {
        size_t part = line->length - done;
        ssize_t n;

        if (part > 4096) {
            part = 4096;
        }

        n = read(fd, buffer + done, part);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            status = -1;
            break;
        }

        if (n == 0) {
            fprintf(stderr, "Unexpected EOF: file may have changed\n");
            status = -1;
            break;
        }

        done += (size_t)n;
    }

    if (status == 0) {
        buffer[done] = '\0';
        printf("%s", buffer);

        if (done > 0 && buffer[done - 1] != '\n') {
            printf("\n");
        }
    }

    free(buffer);
    return status;
}

int main(int argc, char *argv[])
{
    struct Line *table = NULL;
    size_t count = 0;
    size_t i;
    char input[128];
    int fd;
    int status = EXIT_SUCCESS;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    if (build_table(fd, &table, &count) == -1) {
        status = EXIT_FAILURE;
        goto cleanup;
    }

    printf("--- Line table ---\n");
    for (i = 0; i < count; ++i) {
        printf("Line %zu: Offset = %lld, Length = %zu\n",
               i + 1, (long long)table[i].offset, table[i].length);
    }

    while (1) {
        char *end;
        long number;

        printf("Enter line number (0 to quit): ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (ferror(stdin)) {
                perror("stdin");
                status = EXIT_FAILURE;
            }
            break;
        }

        if (strchr(input, '\n') == NULL && !feof(stdin)) {
            int c;

            while ((c = getchar()) != '\n' && c != EOF) {
                /* Discard the rest of an oversized input. */
            }
            fprintf(stderr, "Input is too long\n");
            continue;
        }

        errno = 0;
        number = strtol(input, &end, 10);

        if (end == input || errno == ERANGE) {
            fprintf(stderr, "Invalid line number\n");
            continue;
        }

        while (isspace((unsigned char)*end)) {
            ++end;
        }

        if (*end != '\0' || number < 0) {
            fprintf(stderr, "Invalid line number\n");
            continue;
        }

        if (number == 0) {
            break;
        }

        if ((uintmax_t)number > (uintmax_t)count) {
            fprintf(stderr, "No such line. File contains %zu lines\n",
                    count);
            continue;
        }

        if (print_line(fd, &table[number - 1]) == -1) {
            status = EXIT_FAILURE;
            break;
        }
    }

cleanup:
    free(table);

    if (close(fd) == -1) {
        perror("close");
        status = EXIT_FAILURE;
    }

    return status;
}
