#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit, EXIT_FAILURE, EXIT_SUCCESS
#include <unistd.h>       // fork, execlp, getpid
#include <sys/types.h>    // pid_t
#include <sys/wait.h>     // waitpid, WIFEXITED, WEXITSTATUS

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s file\n", argv[0]);
        return 1;
    }

    pid_t pid;
    int status;

    switch (pid = fork()) {
        case -1:
            perror("fork() error");
            exit(1);

        case 0:
            printf("this is child\n");
            fflush(stdout);
            execlp("cat", "cat", argv[1], (char *)NULL);
            perror("execlp");
            _exit(EXIT_FAILURE);

        default:
            printf("parent is working...\n");

            if (waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
                return EXIT_FAILURE;
            }

            if (WIFEXITED(status)) {
                printf("parent: child exited\n");
            } 
            else {
                printf("parent: child did not exit normally\n");
            }

            printf("parent: child has finished\n");

            return EXIT_SUCCESS;
    }
}
