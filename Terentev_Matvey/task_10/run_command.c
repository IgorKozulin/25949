#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit, EXIT_FAILURE
#include <unistd.h>       // fork, execvp
#include <sys/types.h>    // pid_t
#include <sys/wait.h>     // waitpid, WIFEXITED, WEXITSTATUS
#include <string.h>       // strsignal

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Usage: %s <subcommand> <arguments>\n", argv[0]);
		return 0;
	}
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "Fork failed\n");
		exit(1);
	}
	if (pid == 0) {
		execvp(argv[1], &argv[1]);
		fprintf(stderr, "Command not found");
		exit(127);
	}
	int status;
	pid_t waited = waitpid(pid, &status, 0);
	if (waited < 0){
		perror("waitpid failed");
		exit(2);
	}
	if (WIFEXITED(status)) {
		printf("Child exited with status %d\n", WEXITSTATUS(status));
	} else {
		printf("Child terminated abnormally, signal:\n%d", WTERMSIG(status));
	}
	return 0;
}
