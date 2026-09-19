#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

extern char **environ;

struct Option
{
    char type;
    char *arg;
};

void print_ids(void)
{
    printf("Real UID: %ld\n", (long)getuid());
    printf("Effective UID: %ld\n", (long)geteuid());
    printf("Real GID: %ld\n", (long)getgid());
    printf("Effective GID: %ld", (long)getegid());
}

void make_group_leader(void)
{
    if (setpgid(0, 0) == -1)
    {
        perror("setpgid");
    }
    else
    {
        printf("Process became a process group leader\n");
    }
}

void print_process_ids(void)
{
    printf("PID: %ld\n", (long)getpid());
    printf("PPID: %ld\n", (long)getppid());
    printf("PGID: %ld\n", (long)getpgrp());
}

void print_ulimit(void)
{
    struct rlimit limit;

    if (getrlimit(RLIMIT_NOFILE, &limit) == -1)
    {
        perror("getrlimit(RLIMIT_NOFILE)");
        return;
    }
    printf("Ulimit: ");
    if (limit.rlim_cur == RLIM_INFINITY)
    {
        printf("unlimited\n");
    }
    else
    {
        printf("%llu\n", (unsigned long long)limit.rlim_cur);
    }
}

int parse_limit(const char *str, rlim_t *value)
{
    char *end;
    long long number;
    if (str == NULL || *str == '\0')
    {
        fprintf(stderr, "Error: empty value\n");
        return -1;
    }

    errno = 0;
    end = NULL;
    number = strtoll(str, &end, 10);

    if (errno == ERANGE || end == str || *end != '\0' || number < 0)
    {
        fprintf(stderr, "Error: invalid value '%s'\n", str);
        return -1;
    }
    *value = (rlim_t)number;

    return 0;
}

void set_ulimit_value(const char *str)
{
    struct rlimit limit;
    rlim_t value;

    if (parse_limit(str, &value) == -1)
    {
        return;
    }

    if (getrlimit(RLIMIT_NOFILE, &limit) == -1)
    {
        perror("getrlimit(RLIMIT_NOFILE)");
        return;
    }

    limit.rlim_cur = value;

    if (setrlimit(RLIMIT_NOFILE, &limit) == -1)
    {
        perror("setrlimit(RLIMIT_NOFILE)");
        return;
    }

    printf("Ulimit changed to: %llu\n", (unsigned long long)value);
}

void print_core_limit(void)
{
    struct rlimit limit;

    if (getrlimit(RLIMIT_CORE, &limit) == -1)
    {
        perror("getrlimit(RLIMIT_CORE)");
        return;
    }

    printf("Core file size: ");
    if (limit.rlim_cur == RLIM_INFINITY)
    {
        printf("unlimited\n");
    }
    else
    {
        printf("%llu bytes\n", (unsigned long long)limit.rlim_cur);
    }
}

void set_core_limit(const char *str)
{
    struct rlimit limit;
    rlim_t value;

    if (parse_limit(str, &value) == -1)
    {
        return;
    }

    if (getrlimit(RLIMIT_CORE, &limit) == -1)
    {
        perror("getrlimit(RLIMIT_CORE)");
        return;
    }

    limit.rlim_cur = value;

    if (setrlimit(RLIMIT_CORE, &limit) == -1)
    {
        perror("setrlimit(RLIMIT_CORE)");
        return;
    }

    printf("Core file size changed to: %llu bytes\n", (unsigned long long)value);
}

void print_directory(void)
{
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        return;
    }

    printf("Current directory: %s\n", cwd);
}

void print_environment(void)
{
    char **env = environ;

    while (*env != NULL)
    {
        printf("%s\n", *env);
        env++;
    }
}

void set_environment(const char *value)
{
    if (value == NULL || strchr(value, '=') == NULL)
    {
        fprintf(stderr, "Error: environment variables must have form NAME=value\n");
        return;
    }

    if (putenv((char *)value) != 0)
    {
        perror("putenv");
        return;
    }
    printf("Environment variable changed: %s\n", value);
}

void execute_option(struct Option *option)
{
    switch (option->type)
    {
    case 'i':
        print_ids();
        break;
    case 's':
        make_group_leader();
        break;
    case 'p':
        print_process_ids();
        break;
    case 'u':
        print_ulimit();
        break;
    case 'U':
        set_ulimit_value(option->arg);
        break;
    case 'c':
        print_core_limit();
        break;
    case 'C':
        set_core_limit(option->arg);
        break;
    case 'd':
        print_directory();
        break;
    case 'v':
        print_environment();
        break;
    case 'V':
        set_environment(option->arg);
        break;
    default:
        fprintf(stderr, "Internal error: unknown option '%c'\n", option->type);
        break;
    }
}
int main(int argc, char *argv[])
{
    struct Option *options;

    int option_count = 0;
    int c;
    int i;

    const char *option_string = "ispuU:cC:dvV:";

    if (argc == 1)
    {
        printf("No options specified.\n");
        return 0;
    }

    options = malloc((size_t)(argc - 1) * sizeof(struct Option));

    if (options == NULL)
    {
        perror("malloc");
        return 1;
    }

    optind = 1;
    while ((c = getopt(argc, argv, option_string)) != -1)
    {
        if (c == '?')
        {
            if (optopt != 0)
            {
                fprintf(stderr, "Invalid option: -%c\n", optopt);
            }
            else
            {
                fprintf(stderr, "Invalid option\n");
            }
            free(options);
            return 1;
        }
        if (c == ':')
        {
            fprintf(stderr, "Option -%c requires an argument\n", optopt);
            free(options);
            return 1;
        }
        options[option_count].type = (char)c;
        if (c == 'U' || c == 'C' || c == 'V')
        {
            options[option_count].arg = optarg;
        }
        else
        {
            options[option_count].arg = NULL;
        }
        option_count++;
    }

    if (optind < argc)
    {
        fprintf(stderr, "Unexpected argument:  %s\n", argv[optind]);
        free(options);
        return 1;
    }

    for (i = option_count - 1; i >= 0; i--)
    {
        execute_option(&options[i]);
    }

    free(options);
    return 0;
}