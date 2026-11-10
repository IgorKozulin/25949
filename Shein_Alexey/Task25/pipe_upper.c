#define _XOPEN_SOURCE 600

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int write_all(int fd, const void *data, size_t length)
{
    const unsigned char *p = data;

    while (length > 0) {
        ssize_t n = write(fd, p, length);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            errno = EIO;
            return -1;
        }

        p += n;
        length -= (size_t)n;
    }

    return 0;
}

static int send_text(int fd)
{
    char buffer[4096];
    int at_line_start = 1;

    if (isatty(STDIN_FILENO)) {
        fprintf(stderr, "Enter text. Empty line or Ctrl+D to finish.\n");
    }

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t length = strlen(buffer);

        if (at_line_start && buffer[0] == '\n') {
            break;
        }

        if (write_all(fd, buffer, length) == -1) {
            perror("write to pipe");
            return EXIT_FAILURE;
        }

        at_line_start = length > 0 && buffer[length - 1] == '\n';
    }

    if (ferror(stdin)) {
        perror("stdin");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int print_uppercase(int fd)
{
    unsigned char buffer[4096];

    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        ssize_t i;

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("read from pipe");
            return EXIT_FAILURE;
        }
        if (n == 0) {
            return EXIT_SUCCESS;
        }

        for (i = 0; i < n; ++i) {
            buffer[i] = (unsigned char)toupper(buffer[i]);
        }

        if (write_all(STDOUT_FILENO, buffer, (size_t)n) == -1) {
            perror("write to stdout");
            return EXIT_FAILURE;
        }
    }
}

int main(void)
{
    int pipefd[2];
    pid_t child;
    pid_t waited;
    int child_status;
    int result;
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_IGN;

    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("configure SIGPIPE");
        return EXIT_FAILURE;
    }

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    child = fork();
    if (child == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (child == 0) {
        if (close(pipefd[1]) == -1) {
            perror("child: close write end");
            _exit(EXIT_FAILURE);
        }

        result = print_uppercase(pipefd[0]);
        close(pipefd[0]);
        _exit(result);
    }

    close(pipefd[0]);
    result = send_text(pipefd[1]);

    if (close(pipefd[1]) == -1) {
        perror("parent: close write end");
        result = EXIT_FAILURE;
    }

    do {
        waited = waitpid(child, &child_status, 0);
    } while (waited == -1 && errno == EINTR);

    if (waited == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
        fprintf(stderr, "Child process failed.\n");
        return EXIT_FAILURE;
    }

    return result;
}
