#include <stdio.h>

int main(void)
{
    int *p = NULL;
    *p = 42;          
    printf("unreachable: %d\n", *p);
    return 0;
}
