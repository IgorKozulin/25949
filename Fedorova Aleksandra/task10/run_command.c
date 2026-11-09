#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit, EXIT_FAILURE
#include <unistd.h>       // fork, execvp
#include <sys/types.h>    // pid_t
#include <sys/wait.h>     // waitpid, WIFEXITED, WEXITSTATUS
#include <string.h>       // strsignal

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s command\n", argv[0]);
        return 1;
    }

    pid_t pid;
    int status;

    switch (pid = fork()) {
        case -1:
            perror("fork() error");
            exit(1);

        case 0:
            execvp(argv[1], &argv[1]);
            perror("command not found\n");
            exit(127);

        default:
            printf("parent is working...\n");

            if (waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
                return EXIT_FAILURE;
            }

            if (WIFEXITED(status)) {
                printf("parent: command exited normally with code: %d\n", WEXITSTATUS(status));
            } 
            else {
                printf("parent: command terminated by: %d\n", WTERMSIG(status), strsignal(WTERMSIG(status)));
            }

            printf("parent: child has finished\n");

            return EXIT_SUCCESS;
    }
}
