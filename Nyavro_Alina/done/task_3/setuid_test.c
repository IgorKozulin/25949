#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

static void print_uids(const char *label) { //lable - "before" или "after"
    printf("--- %s ---\n Real UID = %d, Effective UID = %d\n",
           label, getuid(), geteuid());
}

static void try_open(const char *label) {
    FILE *f = fopen("data.txt", "r");
    if (f == NULL) {
        printf("--- %s ---\nfopen(data.txt) FAILED: ", label);
        perror("");
    } 
    else {
        printf("--- %s ---\nfopen(data.txt) SUCCESS\n", label);
        fclose(f);
    }
}

int main(void) {
    print_uids("before");
    try_open("before");

    //Сбрасываем привилегии, эффективный UID = реальному
    if (setuid(getuid()) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    print_uids("after");
    try_open("after");

    return 0;
}