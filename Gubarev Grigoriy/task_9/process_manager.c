#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int parse_count(const char *str)
{
    char *end;
    long val;

    errno = 0;
    val = strtol(str, &end, 10);
    if (errno == ERANGE || end == str || *end != '\0' || val < 1 || val > INT_MAX)
        return -1;
    return (int)val;
}

/*
 * Spawns N child processes (default 1, or set via -n <count>).
 * Each child executes cat(1) on the given file; the parent waits for all
 * children to finish before printing its final summary lines.
 */
int main(int argc, char **argv)
{
    int count = 1;
    int opt;
    const char *file;
    int spawned = 0;
    int i;

    while ((opt = getopt(argc, argv, "n:")) != -1) {
        if (opt == 'n') {
            count = parse_count(optarg);
            if (count == -1) {
                fprintf(stderr, "Invalid process count: %s\n", optarg);
                return EXIT_FAILURE;
            }
        } else {
            fprintf(stderr, "Usage: %s [-n count] <file>\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind + 1 != argc) {
        fprintf(stderr, "Usage: %s [-n count] <file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    file = argv[optind];

    fflush(stdout);
    for (i = 0; i < count; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            break;
        }
        if (pid == 0) {
            execlp("cat", "cat", file, (char *)NULL);
            perror("execlp cat");
            _exit(127);
        }
        ++spawned;
    }

    for (i = 0; i < spawned; ++i) {
        int status;
        pid_t finished;

        while ((finished = wait(&status)) == -1) {
            if (errno != EINTR) {
                perror("wait");
                return EXIT_FAILURE;
            }
        }
        if (WIFEXITED(status)) {
            printf("[PARENT] Child PID %ld exited with status: %d\n",
                   (long)finished, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("[PARENT] Child PID %ld terminated by signal: %d\n",
                   (long)finished, WTERMSIG(status));
        }
    }

    if (spawned < count)
        return EXIT_FAILURE;

    printf("[PARENT] All %d child process(es) finished. This is the last line printed by parent.\n",
           spawned);
    return EXIT_SUCCESS;
}
