#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <errno.h>

#define MAX_ACTIONS 256

enum action_type {
    ACT_PRINT_IDS,
    ACT_SETPGID,
    ACT_PRINT_PIDS,
    ACT_PRINT_ULIMIT,
    ACT_CHANGE_ULIMIT,
    ACT_PRINT_CORE,
    ACT_CHANGE_CORE,
    ACT_PRINT_CWD,
    ACT_PRINT_ENV,
    ACT_SET_ENV
};

struct action {
    enum action_type type;
    char *arg;
};

struct action actions[MAX_ACTIONS];
int action_count = 0;

void add_action(enum action_type type, char *arg) {
    if (action_count < MAX_ACTIONS) {
        actions[action_count].type = type;
        actions[action_count].arg = arg;
        action_count++;
    } else {
        fprintf(stderr, "Too many options\n");
        exit(1);
    }
}

void do_print_ids() {
    printf("Real UID: %d, Effective UID: %d\n", getuid(), geteuid());
    printf("Real GID: %d, Effective GID: %d\n", getgid(), getegid());
}

void do_setpgid() {
    if (setpgid(0, 0) == -1) {
        perror("setpgid");
    } else {
        printf("Process became process group leader. PGID: %d\n", getpgrp());
    }
}

void do_print_pids() {
    printf("PID: %d, PPID: %d, PGID: %d\n", getpid(), getppid(), getpgrp());
}

void do_print_ulimit() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == 0) {
        printf("Core file size (RLIMIT_CORE): ");
        if (rl.rlim_cur == RLIM_INFINITY)
            printf("unlimited\n");
        else
            printf("%lu\n", (unsigned long)rl.rlim_cur);
    } else {
        perror("getrlimit");
    }
}

void do_change_ulimit(char *arg) {
    struct rlimit rl;
    long new_limit;
    char *endptr;

    new_limit = strtol(arg, &endptr, 10);
    if (*endptr != '\0' || new_limit < 0) {
        fprintf(stderr, "Invalid ulimit value: %s\n", arg);
        return;
    }

    if (getrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("getrlimit");
        return;
    }

    rl.rlim_cur = (rlim_t)new_limit;
    if (setrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("setrlimit");
    } else {
        printf("Core file size limit changed to %ld\n", new_limit);
    }
}

void do_print_core() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_CORE, &rl) == 0) {
        printf("Core file size: ");
        if (rl.rlim_cur == RLIM_INFINITY)
            printf("unlimited\n");
        else
            printf("%lu bytes\n", (unsigned long)rl.rlim_cur);
    } else {
        perror("getrlimit");
    }
}

void do_change_core(char *arg) {
    struct rlimit rl;
    long new_size;
    char *endptr;

    new_size = strtol(arg, &endptr, 10);
    if (*endptr != '\0' || new_size < 0) {
        fprintf(stderr, "Invalid core size: %s\n", arg);
        return;
    }

    if (getrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("getrlimit");
        return;
    }

    rl.rlim_cur = (rlim_t)new_size;
    if (setrlimit(RLIMIT_CORE, &rl) == -1) {
        perror("setrlimit");
    } else {
        printf("Core file size limit changed to %ld bytes\n", new_size);
    }
}

void do_print_cwd() {
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        perror("getcwd");
    } else {
        printf("Current working directory: %s\n", cwd);
        free(cwd);
    }
}

extern char **environ;
void do_print_env() {
    char **env = environ;
    printf("Environment variables:\n");
    while (*env) {
        printf("%s\n", *env);
        env++;
    }
}

void do_set_env(char *arg) {
    char *eq = strchr(arg, '=');
    if (eq == NULL) {
        fprintf(stderr, "Invalid format for -V. Use name=value\n");
        return;
    }

    static char *env_strings[MAX_ACTIONS];
    static int env_idx = 0;
    char *new_env = strdup(arg);
    
    if (new_env == NULL) {
        perror("strdup");
        return;
    }
    
    env_strings[env_idx++] = new_env;

    if (putenv(new_env) != 0) {
        perror("putenv");
    } else {
        printf("Environment variable set: %s\n", arg);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    const char *optstring = ":ispuU:cC:dvV:";

    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
            case 'i':
                add_action(ACT_PRINT_IDS, NULL);
                break;
            case 's':
                add_action(ACT_SETPGID, NULL);
                break;
            case 'p':
                add_action(ACT_PRINT_PIDS, NULL);
                break;
            case 'u':
                add_action(ACT_PRINT_ULIMIT, NULL);
                break;
            case 'U':
                add_action(ACT_CHANGE_ULIMIT, optarg);
                break;
            case 'c':
                add_action(ACT_PRINT_CORE, NULL);
                break;
            case 'C':
                add_action(ACT_CHANGE_CORE, optarg);
                break;
            case 'd':
                add_action(ACT_PRINT_CWD, NULL);
                break;
            case 'v':
                add_action(ACT_PRINT_ENV, NULL);
                break;
            case 'V':
                add_action(ACT_SET_ENV, optarg);
                break;
            case ':':
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                return 1;
            case '?':
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                return 1;
            default:
                break;
        }
    }

    for (int i = action_count - 1; i >= 0; i--) {
        switch (actions[i].type) {
            case ACT_PRINT_IDS:
                do_print_ids();
                break;
            case ACT_SETPGID:
                do_setpgid();
                break;
            case ACT_PRINT_PIDS:
                do_print_pids();
                break;
            case ACT_PRINT_ULIMIT:
                do_print_ulimit();
                break;
            case ACT_CHANGE_ULIMIT:
                do_change_ulimit(actions[i].arg);
                break;
            case ACT_PRINT_CORE:
                do_print_core();
                break;
            case ACT_CHANGE_CORE:
                do_change_core(actions[i].arg);
                break;
            case ACT_PRINT_CWD:
                do_print_cwd();
                break;
            case ACT_PRINT_ENV:
                do_print_env();
                break;
            case ACT_SET_ENV:
                do_set_env(actions[i].arg);
                break;
        }
    }

    return 0;
}
