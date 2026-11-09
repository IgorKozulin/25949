#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit, EXIT_FAILURE
#include <unistd.h>       // fork, execlp, getpid
#include <sys/types.h>    // pid_t
#include <sys/wait.h>     // waitpid, WIFEXITED, WEXITSTATUS

int main(int argc, char *argv[])
{
    pid_t pid;
    int status;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("[PARENT] Starting program. My pid: %d\n", getpid());
    fflush(stdout);

    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        printf("{CHILD} I am the child process. My PID: %d\n", getpid());
        fflush(stdout);
        execlp("cat", "cat", argv[1], NULL);

        perror("execlp");
        exit(EXIT_FAILURE);
    } else
    {
        printf("[PARENT] I am the parent. Child PID: %d\n", pid);
        printf("[PARENT] Printing some text while child is working...\n");

        printf("[PARENT] Waiting for child to finish...\n");
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status)) {
            printf("[PARENT] Child exited with status: %d\n",
                   WEXITSTATUS(status));
        } else {
            printf("[PARENT] Child terminated abnormally\n");
        }
        printf("[PARENT] Child has finished. This is the last line printed by parent.\n");

    }
    return 0;
}
