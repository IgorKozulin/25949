#define _XOPEN_SOURCE 600

#ifdef __sun
#define _POSIX_PTHREAD_SEMANTICS 1
#endif

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    sigset_t signals;
    uintmax_t beep_count = 0;

    if (sigemptyset(&signals) == -1 ||
        sigaddset(&signals, SIGINT) == -1 ||
        sigaddset(&signals, SIGQUIT) == -1) {
        perror("signal set");
        return EXIT_FAILURE;
    }

    if (sigprocmask(SIG_BLOCK, &signals, NULL) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    if (puts("Press Ctrl+C to beep, Ctrl+\\ to quit (default keys).") == EOF ||
        fflush(stdout) == EOF) {
        perror("stdout");
        return EXIT_FAILURE;
    }

    for (;;) {
        int received;
        int error = sigwait(&signals, &received);

        if (error == EINTR) {
            continue;
        }
        if (error != 0) {
            fprintf(stderr, "sigwait: %s\n", strerror(error));
            return EXIT_FAILURE;
        }

        if (received == SIGQUIT) {
            break;
        }

        if (received == SIGINT) {
            if (putchar('\a') == EOF || fflush(stdout) == EOF) {
                perror("stdout");
                return EXIT_FAILURE;
            }
            ++beep_count;
        }
    }

    if (printf("\nTotal beeps: %" PRIuMAX "\n", beep_count) < 0 ||
        fflush(stdout) == EOF) {
        perror("stdout");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
