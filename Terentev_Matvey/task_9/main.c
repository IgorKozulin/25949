#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit, EXIT_FAILURE
#include <unistd.h>       // fork, execlp, getpid
#include <sys/types.h>    // pid_t
#include <sys/wait.h>     // waitpid, WIFEXITED, WEXITSTATUS

int main(int argc, char *argv[]){
	if (argc != 2){
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		return 0;
	}
	
	pid_t pid = fork(); // Dividing the process into two

	if (pid < 0){
		fprintf(stderr, "Fork failed");
		return 1;
	}
	if (pid == 0) { // Child logic
        	printf("[CHILD] I am the child process. My PID: %d\n", getpid());
        	fflush(stdout);

        	execlp("cat", "cat", argv[1], NULL);
        	perror("execlp failed");
        	return 2;
    	}
	printf("[PARENT] Starting program. My PID: %d\n", getpid());
    	printf("[PARENT] I am the parent. Child PID: %d\n", pid);
    	printf("[PARENT] Printing some text while child is working...\n");
    	fflush(stdout);
	
	int status;
    	printf("[PARENT] Waiting for child to finish...\n");
    	fflush(stdout);
	
	pid_t waited = waitpid(pid, &status, 0);
    	if (waited < 0) {
        	perror("waitpid failed");
        	exit(EXIT_FAILURE);
    	}
	
	if (WIFEXITED(status)) {
        	printf("[PARENT] Child exited with status: %d\n",
               WEXITSTATUS(status));
    	} else {
        	printf("[PARENT] Child terminated abnormally\n");
    	}
}
