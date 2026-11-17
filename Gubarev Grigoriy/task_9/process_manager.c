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
 * Without -n the parent waits for the child with waitpid(), so its last
 * line always comes after the end of cat. With -n it does not wait: this
 * is the first part of the task, kept to show why the wait is needed.
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

    printf("[PARENT] Starting program. My PID: %ld\n", (long)getpid());
    /* Flush before fork(): otherwise the unsent text would be copied into the child. */
    fflush(stdout);

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        printf("[CHILD] I am the child process. My PID: %ld\n", (long)getpid());
        /* exec throws away the stdio buffer, so flush before it. */
        fflush(stdout);
        execlp("cat", "cat", file, (char *)NULL);
        /* Reached only if exec failed. _exit() does not flush the copied stdio buffer. */
        perror("execlp cat");
        _exit(127);
    }

    printf("[PARENT] I am the parent. Child PID: %ld\n", (long)pid);
    printf("[PARENT] Printing some text while child is working...\n");
    fflush(stdout);

    if (!wait_for_child) {
        printf("[PARENT] Last line printed by parent (did not wait for child).\n");
        return EXIT_SUCCESS;
    }

    printf("[PARENT] Waiting for child to finish...\n");
    fflush(stdout);
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }
    if (WIFEXITED(status))
        printf("[PARENT] Child exited with status: %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("[PARENT] Child terminated by signal: %d\n", WTERMSIG(status));
    printf("[PARENT] Child has finished. This is the last line printed by parent.\n");
    return EXIT_SUCCESS;
}
