#include <stdio.h>        
#include <stdlib.h>       
#include <unistd.h>       
#include <sys/types.h>   
#include <sys/wait.h>     
#include <string.h>       

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("[PARENT] Starting command: %s\n", argv[1]);
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        execvp(argv[1], &argv[1]);
        perror("Execvp failed");
        exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid failed");
        return EXIT_FAILURE;
    }

    if (WIFEXITED(status)) {
        printf("[PARENT] Command exited normally with code: %d\n",
               WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("[PARENT] Command terminated by signal: %d (%s)\n",
               sig, strsignal(sig));
    }
    return EXIT_SUCCESS;
}
