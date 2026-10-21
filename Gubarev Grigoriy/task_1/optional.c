#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

extern char **environ;

typedef struct {
    int option;
    const char *argument;
} ParsedOption;

static void print_ids(void)
{
    printf("Real UID: %ld\nEffective UID: %ld\nReal GID: %ld\nEffective GID: %ld\n",
           (long)getuid(), (long)geteuid(), (long)getgid(), (long)getegid());
}

static void print_process_ids(void)
{
    printf("PID: %ld\nPPID: %ld\nPGID: %ld\n",
           (long)getpid(), (long)getppid(), (long)getpgrp());
}

static void print_limit(int resource, const char *label, int bytes)
{
    struct rlimit limit;
    if (getrlimit(resource, &limit) == -1) {
        perror("getrlimit");
        return;
    }
    printf("%s: ", label);
    if (limit.rlim_cur == RLIM_INFINITY)
        printf("unlimited");
    else if (bytes)
        printf("%llu bytes", (unsigned long long)limit.rlim_cur);
    else
        printf("%llu", (unsigned long long)limit.rlim_cur);
    putchar('\n');
}

static int parse_limit(const char *text, rlim_t *value)
{
    char *end;
    unsigned long long parsed;

    if (text == NULL || *text == '\0' || *text == '-')
        return -1;
    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || parsed > (unsigned long long)RLIM_INFINITY)
        return -1;
    *value = (rlim_t)parsed;
    return 0;
}

static int set_limit(int resource, const char *text, const char *label)
{
    struct rlimit limit;
    rlim_t value;

    if (parse_limit(text, &value) == -1) {
        fprintf(stderr, "Invalid %s value: '%s'\n", label, text ? text : "");
        return -1;
    }
    if (getrlimit(resource, &limit) == -1) {
        perror("getrlimit");
        return -1;
    }
    if (value > limit.rlim_max) {
        fprintf(stderr, "%s value exceeds the hard limit\n", label);
        return -1;
    }
    limit.rlim_cur = value;
    if (setrlimit(resource, &limit) == -1) {
        perror("setrlimit");
        return -1;
    }
    printf("%s changed\n", label);
    return 0;
}

static int apply_option(const ParsedOption *item)
{
    char cwd[PATH_MAX];
    char **entry;

    switch (item->option) {
    case 'i': print_ids(); break;
    case 's':
        if (setpgid(0, 0) == -1) { perror("setpgid"); return -1; }
        printf("Process is now a process-group leader\n");
        break;
    case 'p': print_process_ids(); break;
    case 'u': print_limit(RLIMIT_NOFILE, "Open-file limit", 0); break;
    case 'U': if (set_limit(RLIMIT_NOFILE, item->argument, "Open-file limit") == -1) return -1; break;
    case 'c': print_limit(RLIMIT_CORE, "Core-file size limit", 1); break;
    case 'C': if (set_limit(RLIMIT_CORE, item->argument, "Core-file size limit") == -1) return -1; break;
    case 'd':
        if (getcwd(cwd, sizeof(cwd)) == NULL) { perror("getcwd"); return -1; }
        printf("Current directory: %s\n", cwd);
        break;
    case 'v':
        for (entry = environ; entry != NULL && *entry != NULL; ++entry)
            puts(*entry);
        break;
    case 'V': {
        const char *equals = strchr(item->argument, '=');
        size_t name_length;
        char *name;
        if (equals == NULL || equals == item->argument) {
            fprintf(stderr, "Expected -Vname=value\n");
            return -1;
        }
        name_length = (size_t)(equals - item->argument);
        name = malloc(name_length + 1);
        if (name == NULL) { perror("malloc"); return -1; }
        memcpy(name, item->argument, name_length);
        name[name_length] = '\0';
        if (setenv(name, equals + 1, 1) == -1) {
            perror("setenv");
            free(name);
            return -1;
        }
        printf("Environment variable %s set\n", name);
        free(name);
        break;
    }
    default:
        fprintf(stderr, "Internal error: unknown option\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    ParsedOption *options = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int option;
    int failed = 0;

    if (argc == 1) {
        puts("No options specified.");
        return EXIT_SUCCESS;
    }

    opterr = 0;
    while ((option = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {
        ParsedOption *grown;
        if (option == '?') {
            fprintf(stderr, "Unknown option or missing argument: -%c\n", optopt);
            free(options);
            return EXIT_FAILURE;
        }
        if (count == capacity) {
            size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
            grown = realloc(options, new_capacity * sizeof(*options));
            if (grown == NULL) {
                perror("realloc");
                free(options);
                return EXIT_FAILURE;
            }
            options = grown;
            capacity = new_capacity;
        }
        options[count].option = option;
        options[count].argument = optarg;
        ++count;
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        free(options);
        return EXIT_FAILURE;
    }

    while (count > 0) {
        if (apply_option(&options[--count]) == -1)
            failed = 1;
    }
    free(options);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
