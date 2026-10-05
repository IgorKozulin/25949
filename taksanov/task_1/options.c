#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

#define MAX_OPTIONS 256

extern char **environ;

typedef struct {
    char type;
    char *arg;
} Option;

// -i: Реальные и эффективные UID/GID
void print_ids(void) {
    printf("Real UID: %ld, Effective UID: %ld\n", (long)getuid(), (long)geteuid());
    printf("Real GID: %ld, Effective GID: %ld\n", (long)getgid(), (long)getegid());
}

// -s: Процесс становится лидером новой группы
void make_group_leader(void) {
    if (setpgid(0, 0) == -1) {
        perror("setpgid");
    } else {
        printf("Process became group leader. New PGID: %ld\n", (long)getpgrp());
    }
}

// -p: PID, родительский PPID и PGID группы
void print_process_ids(void) {
    printf("PID: %ld, PPID: %ld, PGID: %ld\n", 
           (long)getpid(), (long)getppid(), (long)getpgrp());
}

// Вспомогательная функция безопасного парсинга чисел
int parse_number(const char *str, rlim_t *value) {
    char *end = NULL;
    errno = 0;
    long long number = strtoll(str, &end, 10);

    if (errno == ERANGE || end == str || *end != '\0' || number < 0) {
        fprintf(stderr, "Error: Invalid number value '%s'\n", str);
        return -1;
    }
    *value = (rlim_t)number;
    return 0;
}

// -u: Печать ulimit (RLIMIT_NOFILE)
void print_ulimit(void) {
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        perror("getrlimit(RLIMIT_NOFILE)");
        return;
    }
    printf("Ulimit (RLIMIT_NOFILE): ");
    if (limit.rlim_cur == RLIM_INFINITY) {
        printf("unlimited\n");
    } else {
        printf("%llu\n", (unsigned long long)limit.rlim_cur);
    }
}

// -U: Изменение ulimit
void set_ulimit_value(const char *str) {
    rlim_t value;
    if (parse_number(str, &value) == -1) return;

    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
        perror("getrlimit(RLIMIT_NOFILE)");
        return;
    }

    limit.rlim_cur = value;
    if (setrlimit(RLIMIT_NOFILE, &limit) == -1) {
        perror("setrlimit(RLIMIT_NOFILE)");
        return;
    }
    printf("Ulimit changed to: %llu\n", (unsigned long long)value);
}

// -c: Размер core-файла
void print_core_limit(void) {
    struct rlimit limit;
    if (getrlimit(RLIMIT_CORE, &limit) == -1) {
        perror("getrlimit(RLIMIT_CORE)");
        return;
    }
    printf("Core file size limit: ");
    if (limit.rlim_cur == RLIM_INFINITY) {
        printf("unlimited\n");
    } else {
        printf("%llu bytes\n", (unsigned long long)limit.rlim_cur);
    }
}

// -C: Изменение размера core-файла
void set_core_limit(const char *str) {
    rlim_t value;
    if (parse_number(str, &value) == -1) return;

    struct rlimit limit;
    if (getrlimit(RLIMIT_CORE, &limit) == -1) {
        perror("getrlimit(RLIMIT_CORE)");
        return;
    }

    limit.rlim_cur = value;
    if (setrlimit(RLIMIT_CORE, &limit) == -1) {
        perror("setrlimit(RLIMIT_CORE)");
        return;
    }
    printf("Core file size changed to: %llu bytes\n", (unsigned long long)value);
}

// -d: Текущая рабочая директория
void print_directory(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return;
    }
    printf("Current directory: %s\n", cwd);
}

// -v: Переменные окружения
void print_environment(void) {
    printf("Environment variables:\n");
    for (char **env = environ; *env != NULL; env++) {
        printf("  %s\n", *env);
    }
}

// -V: Изменение/добавление переменной окружения
void set_environment(const char *value) {
    if (value == NULL || strchr(value, '=') == NULL) {
        fprintf(stderr, "Error: Environment variable must have form NAME=value (got '%s')\n", value ? value : "");
        return;
    }
    if (putenv((char *)value) != 0) {
        perror("putenv");
        return;
    }
    printf("Environment variable changed: %s\n", value);
}

// Функция запуска нужного действия по типу опции
void execute_option(Option *opt) {
    switch (opt->type) {
        case 'i': print_ids(); break;
        case 's': make_group_leader(); break;
        case 'p': print_process_ids(); break;
        case 'u': print_ulimit(); break;
        case 'U': set_ulimit_value(opt->arg); break;
        case 'c': print_core_limit(); break;
        case 'C': set_core_limit(opt->arg); break;
        case 'd': print_directory(); break;
        case 'v': print_environment(); break;
        case 'V': set_environment(opt->arg); break;
        default:
            fprintf(stderr, "Unknown option '%c'\n", opt->type);
            break;
    }
}

int main(int argc, char *argv[]) {
    Option options[MAX_OPTIONS];
    int option_count = 0;
    int c;

    if (argc == 1) {
        printf("No options specified.\n");
        return 0;
    }

    // 1. Сбор опций через getopt (читает слева направо)
    while ((c = getopt(argc, argv, "ispuU:cC:dvV:")) != -1) {
        if (c == '?') {
            fprintf(stderr, "Invalid option: -%c\n", optopt);
            return 1;
        }

        if (option_count >= MAX_OPTIONS) {
            fprintf(stderr, "Error: Too many options (maximum is %d)\n", MAX_OPTIONS);
            return 1;
        }

        options[option_count].type = (char)c;
        options[option_count].arg = optarg;
        option_count++;
    }

    // Проверка на лишние позиционные аргументы (например, ./options -i test)
    if (optind < argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        return 1;
    }

    // 2. ВЫПОЛНЕНИЕ СПРАВА НАЛЕВО (обратный цикл)
    for (int i = option_count - 1; i >= 0; i--) {
        execute_option(&options[i]);
    }

    return 0;
}
