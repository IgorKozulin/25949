#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // All options: s, p, u, d don't need arguments
    char *options = "ispuU:cC:dvV:";
    int c;
    
    // Parse options
    while ((c = getopt(argc, argv, options)) != -1) {
        switch (c) {
            case 'i':
                printf("Option -i detected\n");
                break;
            case 's':
                printf("Option -s detected\n");
                break;
            case 'p':
                printf("Option -p detected\n");
                break;
            case 'u':
                printf("Option -u detected\n");
                break;
            case 'U':
                printf("Option -U with value: %s\n", optarg);
                break;
            case 'c':
                printf("Option -c detected\n");
                break;
            case 'C':
                printf("Option -C with value: %s\n", optarg);
                break;
            case 'd':
                printf("Option -d detected\n");
                break;
            case 'v':
                printf("Option -v detected\n");
                break;
            case 'V':
                printf("Option -V with value: %s\n", optarg);
                break;
            case '?':
                printf("Invalid option: %c\n", optopt);
                break;
        }
    }
    
    return 0;
}

