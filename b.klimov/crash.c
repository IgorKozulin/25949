/* crash.c */
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    volatile int *bad_ptr = NULL;
    printf("Crash-программа: вызываю разыменование NULL...\n");
    *bad_ptr = 42;
    return 0;
}