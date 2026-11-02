#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

void print_uids()
{
    printf("Real UID: %ld\n", (long)getuid());
    printf("Effective UID: %ld\n", (long)geteuid());
}

void try_open_file(void)
{
    FILE *file = fopen("data.txt", "r");

    if (file == NULL) {
        perror("fopen data.txt");
        printf("access denied or file unavailable\n");
        return;
    }

    printf("file opened successfully\n");
    fclose(file);
}

int main(void)
{
    printf("trying to open data.txt\n");
    try_open_file();

    printf("\ncalling setuid(getuid())\n");
    
    if (setuid(getuid()) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    printf("trying to open data.txt again\n");
    try_open_file();

    print_uids();

    return EXIT_SUCCESS;
}

