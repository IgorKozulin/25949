#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

static void print_ids(const char *stage)
{
    printf("%s: real UID=%ld, effective UID=%ld\n",
           stage, (long)getuid(), (long)geteuid());
}

static int try_open_data(void)
{
    FILE *file = fopen("data.txt", "r");
    if (file == NULL) {
        perror("fopen data.txt");
        return -1;
    }
    puts("data.txt opened successfully");
    if (fclose(file) == EOF) {
        perror("fclose");
        return -1;
    }
    return 0;
}

int main(void)
{
    int first_result;

    print_ids("Before setuid");
    first_result = try_open_data();

    puts("Calling setuid(getuid()) to drop any setuid privilege");
    if (setuid(getuid()) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    print_ids("After setuid");
    if (try_open_data() == -1)
        return EXIT_FAILURE;
    return first_result == -1 ? EXIT_FAILURE : EXIT_SUCCESS;
}
