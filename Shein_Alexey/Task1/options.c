#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

extern char **environ;

struct option {
    int name;
    char *arg;
};

/* Print or change the soft resource limit. */
static int resource_limit(int resource, const char *arg,
                          const char *label)
{
    struct rlimit limit;
    char *end;
    long value;

    if (getrlimit(resource, &limit) == -1) {
        perror("getrlimit");
        return -1;
    }

    if (arg != NULL) {
        errno = 0;
        value = strtol(arg, &end, 10);

        if (arg[0] < '0' || arg[0] > '9' ||
            *end != '\0' || errno == ERANGE || value < 0) {
            fprintf(stderr, "Invalid limit: %s\n", arg);
            return -1;
        }

        if ((unsigned long long)(rlim_t)value !=
            (unsigned long long)value ||
            (rlim_t)value == RLIM_INFINITY) {
            fprintf(stderr, "Limit is out of range: %s\n", arg);
            return -1;
        }

        if (limit.rlim_max != RLIM_INFINITY &&
            (rlim_t)value > limit.rlim_max) {
            fprintf(stderr, "Limit exceeds the hard limit: %s\n", arg);
            return -1;
        }

        limit.rlim_cur = (rlim_t)value;

        if (setrlimit(resource, &limit) == -1) {
            perror("setrlimit");
            return -1;
        }
    }

    if (limit.rlim_cur == RLIM_INFINITY) {
        printf("%s: unlimited\n", label);
    } else {
        printf("%s: %llu\n", label,
               (unsigned long long)limit.rlim_cur);
    }

    return 0;
}

static int execute_option(int name, char *arg)
{
    char cwd[PATH_MAX];
    char **env;
    char *equals;

    switch (name) {
    case 'i':
        printf("UID=%lu EUID=%lu GID=%lu EGID=%lu\n",
               (unsigned long)getuid(),
               (unsigned long)geteuid(),
               (unsigned long)getgid(),
               (unsigned long)getegid());
        break;

    case 's':
        if (setpgid(0, 0) == -1) {
            perror("setpgid");
            return -1;
        }
        puts("Process is a process group leader");
        break;

    case 'p':
        printf("PID=%ld PPID=%ld PGID=%ld\n",
               (long)getpid(), (long)getppid(), (long)getpgrp());
        break;

    case 'u':
    case 'U':
        return resource_limit(RLIMIT_NOFILE,
                              name == 'U' ? arg : NULL,
                              "Open file limit");

    case 'c':
    case 'C':
        return resource_limit(RLIMIT_CORE,
                              name == 'C' ? arg : NULL,
                              "Core file limit (bytes)");

    case 'd':
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd");
            return -1;
        }
        printf("Current directory: %s\n", cwd);
        break;

    case 'v':
        for (env = environ; *env != NULL; ++env) {
            puts(*env);
        }
        break;

    case 'V':
        equals = strchr(arg, '=');
        if (equals == NULL || equals == arg) {
            fprintf(stderr, "Expected -VNAME=value\n");
            return -1;
        }

        /* argv strings remain valid until the program exits. */
        if (putenv(arg) != 0) {
            perror("putenv");
            return -1;
        }
        break;
    }

    return 0;
}

static void usage(const char *program)
{
    printf("Usage: %s [options]\n"
           "Options are executed from right to left.\n"
           "  -i             Print real and effective UID/GID\n"
           "  -s             Become a process group leader\n"
           "  -p             Print PID, PPID and PGID\n"
           "  -u             Print open file limit\n"
           "  -U number      Set open file limit\n"
           "  -c             Print core file limit in bytes\n"
           "  -C number      Set core file limit in bytes\n"
           "  -d             Print current directory\n"
           "  -v             Print environment\n"
           "  -V NAME=value  Set environment variable\n",
           program);
}

int main(int argc, char *argv[])
{
    struct option *list = NULL;
    struct option *new_list;
    size_t count = 0;
    int c;
    int status = EXIT_SUCCESS;

    if (argc == 1) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    opterr = 0;

    while ((c = getopt(argc, argv, ":ispuU:cC:dvV:")) != -1) {
        if (c == '?' || c == ':') {
            if (c == ':') {
                fprintf(stderr, "Option -%c requires an argument\n",
                        optopt);
            } else {
                fprintf(stderr, "Unknown option: -%c\n", optopt);
            }
            free(list);
            return EXIT_FAILURE;
        }

        new_list = realloc(list, (count + 1) * sizeof(*list));
        if (new_list == NULL) {
            perror("realloc");
            free(list);
            return EXIT_FAILURE;
        }

        list = new_list;
        list[count].name = c;
        list[count].arg =
            (c == 'U' || c == 'C' || c == 'V') ? optarg : NULL;
        ++count;
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        free(list);
        return EXIT_FAILURE;
    }

    while (count > 0) {
        --count;
        fflush(stdout);

        if (execute_option(list[count].name, list[count].arg) != 0) {
            status = EXIT_FAILURE;
        }
    }

    free(list);
    return status;
}
