#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit, EXIT_FAILURE
#include <unistd.h>       // fork, execvp
#include <sys/types.h>    // pid_t
#include <sys/wait.h>     // waitpid, WIFEXITED, WEXITSTATUS
#include <string.h>       // strsignal

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int status;
    pid_t pid;
    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        execvp(argv[1], &argv[1]);
        perror("The command not found");
        exit((127));
    }

    if (pid > 0)
    {
        if (waitpid(pid, &status, 0) == -1)
        {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status))
        {
            printf("[PARENT] Command exited normally with code: %d\n", WEXITSTATUS(status));
            fflush(stdout);
        } else if (WIFSIGNALED(status))
        {
            int sig = WTERMSIG(status);
            printf("[PARENT] Command terminated by signal: %d (%s)\n", sig, strsignal(sig));
            fflush(stdout);
        } else
        {
            printf("[PARENT] Command terminated abnormally (unknown status)\n");
        }
    }

    return 0;
}
