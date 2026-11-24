#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * The child runs cat(1) on a file, the parent prints its own text.
 * Without -n the parent waits for the child with waitpid() first, so all
 * parent output appears strictly after the child finishes. With -n it does
 * not wait: kept to demonstrate the first part of the task.
 */
int main(int argc, char **argv)
{
    const char *file;
    int wait_for_child = 1;
    pid_t pid;
    int status;

    if (argc == 2) {
        file = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "-n") == 0) {
        wait_for_child = 0;
        file = argv[2];
    } else {
        fprintf(stderr, "Usage: %s [-n] <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        execlp("cat", "cat", file, (char *)NULL);
        /* Reached only if exec failed. _exit() does not flush the parent's stdio buffer. */
        perror("execlp cat");
        _exit(127);
    }

    if (!wait_for_child) {
        printf("[PARENT] Child PID: %ld (NOT waiting for child)\n", (long)pid);
        printf("[PARENT] Last line printed by parent before child finished.\n");
        return EXIT_SUCCESS;
    }

    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }

    if (WIFEXITED(status))
        printf("[PARENT] Child (PID %ld) exited with status: %d\n", (long)pid, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("[PARENT] Child (PID %ld) terminated by signal: %d\n", (long)pid, WTERMSIG(status));
    printf("[PARENT] Child has finished. This text is printed by parent AFTER child.\n");
    return EXIT_SUCCESS;
}
