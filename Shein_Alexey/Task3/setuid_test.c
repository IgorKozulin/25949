#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static void print_ids(void)
{
    printf("Real UID:      %lu\n", (unsigned long)getuid());
    printf("Effective UID: %lu\n", (unsigned long)geteuid());
}

static void check_file(void)
{
    FILE *file;

    fflush(stdout);

    file = fopen("data.txt", "r");
    if (file == NULL) {
        perror("fopen data.txt");
        return;
    }

    puts("data.txt: opened successfully");

    if (fclose(file) == EOF) {
        perror("fclose");
    }
}

int main(void)
{
    puts("Before setuid:");
    print_ids();
    check_file();

    fflush(stdout);

    if (setuid(getuid()) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    puts("\nAfter setuid:");
    print_ids();
    check_file();

    return EXIT_SUCCESS;
}
