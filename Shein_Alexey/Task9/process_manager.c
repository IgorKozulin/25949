#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    pid_t children[2];
    int created = 0;
    int i;
    int no_wait = 0;
    int result = EXIT_SUCCESS;

    if (argc != 2 &&
        !(argc == 3 && strcmp(argv[2], "--no-wait") == 0)) {
        fprintf(stderr, "Usage: %s filename [--no-wait]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 3) {
        no_wait = 1;
    }

    printf("[PARENT] Starting. PID: %ld\n", (long)getpid());

    if (fflush(stdout) == EOF) {
        perror("fflush");
        return EXIT_FAILURE;
    }

    for (i = 0; i < 2; ++i) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            result = EXIT_FAILURE;
            break;
        }

        if (pid == 0) {
            printf("[CHILD %d] PID: %ld. Running cat.\n",
                   i + 1, (long)getpid());

            if (fflush(stdout) == EOF) {
                perror("fflush");
                _exit(EXIT_FAILURE);
            }

            execlp("cat", "cat", argv[1], (char *)NULL);

            perror("execlp cat");
            _exit(127);
        }

        children[created++] = pid;
    }

    for (i = 0; i < created; ++i) {
        printf("[PARENT] Child %d PID: %ld\n",
               i + 1, (long)children[i]);
    }

    printf("[PARENT] Printing text while children can run.\n");

    if (fflush(stdout) == EOF) {
        perror("fflush");
        result = EXIT_FAILURE;
    }

    if (no_wait) {
        printf("[PARENT] Last parent line WITHOUT waiting.\n");
        return result;
    }

    for (i = 0; i < created; ++i) {
        int status;
        pid_t waited;

        do {
            waited = waitpid(children[i], &status, 0);
        } while (waited == -1 && errno == EINTR);

        if (waited == -1) {
            perror("waitpid");
            result = EXIT_FAILURE;
            continue;
        }

        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);

            printf("[PARENT] Child %d exited with code: %d\n",
                   i + 1, code);

            if (code != 0) {
                result = EXIT_FAILURE;
            }
        } else if (WIFSIGNALED(status)) {
            printf("[PARENT] Child %d terminated by signal: %d\n",
                   i + 1, WTERMSIG(status));
            result = EXIT_FAILURE;
        } else {
            fprintf(stderr,
                    "[PARENT] Unexpected status for child %d\n",
                    i + 1);
            result = EXIT_FAILURE;
        }
    }

    printf("[PARENT] Finished waiting for children. "
           "This is the last parent line.\n");

    return result;
}
