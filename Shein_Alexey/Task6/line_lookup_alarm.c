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
#include <signal.h>

struct Line {
    off_t offset;
    size_t length;
};

static volatile sig_atomic_t timed_fd = -1;

static int write_all(int fd, const char *buffer, size_t length)
{
    size_t done = 0;

    while (done < length) {
        ssize_t n = write(fd, buffer + done, length - done);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (n == 0) {
            return -1;
        }

        done += (size_t)n;
    }

    return 0;
}

static void alarm_handler(int sig)
{
    const char message[] =
        "\n[TIMEOUT] Printing the entire file:\n";
    char buffer[4096];
    int fd = (int)timed_fd;

    (void)sig;

    if (write_all(STDOUT_FILENO, message,
                  sizeof(message) - 1) == -1) {
        _exit(EXIT_FAILURE);
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        _exit(EXIT_FAILURE);
    }

    while (1) {
        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            _exit(EXIT_FAILURE);
        }

        if (n == 0) {
            break;
        }

        if (write_all(STDOUT_FILENO, buffer, (size_t)n) == -1) {
            _exit(EXIT_FAILURE);
        }
    }

    close(fd);
    _exit(EXIT_SUCCESS);
}

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

    while (1) {
        ssize_t n = read(fd, &c, 1);

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

        if (printf("%s", buffer) < 0) {
            status = -1;
        }

        if (done > 0 && buffer[done - 1] != '\n') {
            if (printf("\n") < 0) {
                status = -1;
            }
        }
    }

    free(buffer);
    return status;
}

int main(int argc, char *argv[])
{
    struct Line *table = NULL;
    struct sigaction action;
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

    timed_fd = fd;

    memset(&action, 0, sizeof(action));
    action.sa_handler = alarm_handler;

    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGALRM, &action, NULL) == -1) {
        perror("sigaction");
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

        printf("Enter line number (0 to quit, 5 seconds): ");

        if (fflush(stdout) == EOF) {
            perror("stdout");
            status = EXIT_FAILURE;
            break;
        }

        alarm(5);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            alarm(0);

            if (ferror(stdin)) {
                perror("stdin");
                status = EXIT_FAILURE;
            }
            break;
        }

        if (strchr(input, '\n') == NULL && !feof(stdin)) {
            int c;

            while ((c = getchar()) != '\n' && c != EOF) {
                /* Keep the timer active until the line is complete. */
            }

            alarm(0);

            if (ferror(stdin)) {
                perror("stdin");
                status = EXIT_FAILURE;
                break;
            }

            fprintf(stderr, "Input is too long\n");
            continue;
        }

        alarm(0);

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
    alarm(0);
    free(table);

    if (close(fd) == -1) {
        perror("close");
        status = EXIT_FAILURE;
    }

    return status;
}
