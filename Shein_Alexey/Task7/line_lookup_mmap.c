#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
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
    size_t offset;
    size_t length;
};

static volatile sig_atomic_t expired = 0;

static void alarm_handler(int sig)
{
    (void)sig;
    expired = 1;
}

static int build_table(const char *map, size_t size,
                       struct Line **table, size_t *count)
{
    size_t i;
    size_t start = 0;
    size_t index = 0;

    *count = 0;

    for (i = 0; i < size; ++i) {
        if (map[i] == '\n') {
            ++*count;
        }
    }

    if (map[size - 1] != '\n') {
        ++*count;
    }

    if (*count > SIZE_MAX / sizeof(**table)) {
        fprintf(stderr, "Too many lines\n");
        return -1;
    }

    *table = malloc(*count * sizeof(**table));
    if (*table == NULL) {
        perror("malloc");
        return -1;
    }

    for (i = 0; i < size; ++i) {
        if (map[i] == '\n') {
            (*table)[index].offset = start;
            (*table)[index].length = i - start + 1;
            ++index;
            start = i + 1;
        }
    }

    if (start < size) {
        (*table)[index].offset = start;
        (*table)[index].length = size - start;
    }

    return 0;
}

/*
 * Return values:
 *  1: complete input
 *  0: EOF
 *  2: timeout
 *  3: oversized input
 * -1: error
 *
 * SIGALRM is blocked outside pselect().
 */
static int get_input(char *buffer, size_t capacity,
                     const sigset_t *wait_mask,
                     const sigset_t *alarm_set)
{
    size_t used = 0;
    int too_long = 0;
    int result;
    sigset_t pending;

    expired = 0;
    alarm(5);

    while (1) {
        fd_set readers;
        int ready;
        int c;

        FD_ZERO(&readers);
        FD_SET(STDIN_FILENO, &readers);

        ready = pselect(STDIN_FILENO + 1, &readers,
                        NULL, NULL, NULL, wait_mask);

        if (expired) {
            result = 2;
            break;
        }

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("pselect");
            result = -1;
            break;
        }

        if (ready == 0) {
            continue;
        }

        c = getchar();

        if (c == EOF) {
            if (ferror(stdin)) {
                perror("stdin");
                result = -1;
            } else if (too_long) {
                result = 3;
            } else {
                result = used > 0 ? 1 : 0;
            }
            break;
        }

        if (c == '\n') {
            result = too_long ? 3 : 1;
            break;
        }

        if (used < capacity - 1) {
            buffer[used++] = (char)c;
        } else {
            too_long = 1;
        }
    }

    alarm(0);

    /* Consume an alarm that became pending at the input boundary. */
    if (sigpending(&pending) == -1) {
        perror("sigpending");
        return -1;
    }

    if (sigismember(&pending, SIGALRM) == 1) {
        int sig;
        int error = sigwait(alarm_set, &sig);

        if (error != 0) {
            fprintf(stderr, "sigwait: %s\n", strerror(error));
            return -1;
        }

        result = 2;
    }

    buffer[used] = '\0';
    return result;
}

static int print_text(const char *text, size_t length)
{
    if (fwrite(text, 1, length, stdout) != length) {
        perror("fwrite");
        return -1;
    }

    if (length > 0 && text[length - 1] != '\n') {
        if (putchar('\n') == EOF) {
            perror("stdout");
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct stat info;
    struct Line *table = NULL;
    struct sigaction action;
    sigset_t alarm_set;
    sigset_t old_mask;
    sigset_t wait_mask;
    char *map = MAP_FAILED;
    char input[128];
    size_t size = 0;
    size_t count = 0;
    size_t i;
    int fd = -1;
    int mask_saved = 0;
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

    if (fstat(fd, &info) == -1) {
        perror("fstat");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    if (!S_ISREG(info.st_mode)) {
        fprintf(stderr, "A regular text file is required\n");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    if (info.st_size == 0) {
        puts("File is empty.");
        goto cleanup;
    }

    if (info.st_size < 0 ||
        (uintmax_t)info.st_size > (uintmax_t)SIZE_MAX) {
        fprintf(stderr, "File is too large\n");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    size = (size_t)info.st_size;
    map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (map == MAP_FAILED) {
        perror("mmap");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    {
        int error = close(fd);
        fd = -1;

        if (error == -1) {
            perror("close");
            status = EXIT_FAILURE;
            goto cleanup;
        }
    }

    if (build_table(map, size, &table, &count) == -1) {
        status = EXIT_FAILURE;
        goto cleanup;
    }

    /* Avoid stdio reading ahead of pselect(). */
    if (setvbuf(stdin, NULL, _IONBF, 0) != 0) {
        fprintf(stderr, "Cannot disable stdin buffering\n");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    if (sigemptyset(&alarm_set) == -1 ||
        sigaddset(&alarm_set, SIGALRM) == -1 ||
        sigprocmask(SIG_BLOCK, &alarm_set, &old_mask) == -1) {
        perror("signal mask");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    mask_saved = 1;
    wait_mask = old_mask;
    sigdelset(&wait_mask, SIGALRM);

    memset(&action, 0, sizeof(action));
    action.sa_handler = alarm_handler;

    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGALRM, &action, NULL) == -1) {
        perror("sigaction");
        status = EXIT_FAILURE;
        goto cleanup;
    }

    puts("--- Line table ---");

    for (i = 0; i < count; ++i) {
        printf("Line %zu: Offset = %zu, Length = %zu\n",
               i + 1, table[i].offset, table[i].length);
    }

    while (1) {
        char *end;
        long number;
        int result;

        printf("Enter line number (0 to quit, 5 seconds): ");

        if (fflush(stdout) == EOF) {
            perror("stdout");
            status = EXIT_FAILURE;
            break;
        }

        result = get_input(input, sizeof(input),
                           &wait_mask, &alarm_set);

        if (result == 2) {
            puts("\n[TIMEOUT] Printing the entire file:");

            if (print_text(map, size) == -1) {
                status = EXIT_FAILURE;
            }
            break;
        }

        if (result == 0) {
            break;
        }

        if (result == -1) {
            status = EXIT_FAILURE;
            break;
        }

        if (result == 3) {
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

        {
            const struct Line *line = &table[number - 1];

            if (print_text(map + line->offset, line->length) == -1) {
                status = EXIT_FAILURE;
                break;
            }
        }
    }

cleanup:
    alarm(0);

    if (fflush(stdout) == EOF) {
        perror("stdout");
        status = EXIT_FAILURE;
    }

    free(table);

    if (map != MAP_FAILED && munmap(map, size) == -1) {
        perror("munmap");
        status = EXIT_FAILURE;
    }

    if (fd != -1 && close(fd) == -1) {
        perror("close");
        status = EXIT_FAILURE;
    }

    if (mask_saved &&
        sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
        perror("sigprocmask");
        status = EXIT_FAILURE;
    }

    return status;
}
