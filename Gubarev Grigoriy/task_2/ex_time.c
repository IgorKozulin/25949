#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    time_t now;
    struct tm *california_time;
    char formatted[64];

    if (setenv("TZ", "America/Los_Angeles", 1) == -1) {
        perror("setenv");
        return EXIT_FAILURE;
    }
    tzset();
    now = time(NULL);
    if (now == (time_t)-1) {
        perror("time");
        return EXIT_FAILURE;
    }
    california_time = localtime(&now);
    if (california_time == NULL) {
        perror("localtime");
        return EXIT_FAILURE;
    }
    if (strftime(formatted, sizeof(formatted), "%m/%d/%Y %H:%M %Z", california_time) == 0) {
        fprintf(stderr, "Could not format the time\n");
        return EXIT_FAILURE;
    }
    puts(formatted);
    return EXIT_SUCCESS;
}
