#include <stdio.h>

int main(void) {
    printf("About to crash...\n");
    fflush(stdout);

    int *p = NULL;
    *p = 42;   /* segmentation fault */

    return 0;
}