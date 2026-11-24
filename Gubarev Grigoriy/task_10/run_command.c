#define _POSIX_C_SOURCE 200809L
#ifdef __sun
#define __EXTENSIONS__ 1 /* strsignal() is outside strict C99 on Solaris */
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Runs argv[1] with the remaining arguments as a child process, waits for it
 * and prints how it ended: an exit code or the signal that killed it.
 */
int main(int argc, char **argv)
{
    pid_t pid;
    int status;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s command [arguments...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("[PARENT] Starting command: %s\n", argv[1]);
    /* Flush before fork(): otherwise the child would carry a copy of this text. */
    fflush(stdout);

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        /* argv[1] is the command, &argv[1] is its argv; the array ends with NULL. */
        execvp(argv[1], &argv[1]);
        /* Reached only if the command could not be started. */
        perror("Execvp failed");
        _exit(127); /* the usual "command not found" code */
    }

    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
    }

    if (WIFEXITED(status)) {
        printf("[PARENT] Command exited normally with code: %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        int signal_number = WTERMSIG(status);
        printf("[PARENT] Command terminated by signal: %d (%s)\n",
               signal_number, strsignal(signal_number));
    }
    return EXIT_SUCCESS;
}
