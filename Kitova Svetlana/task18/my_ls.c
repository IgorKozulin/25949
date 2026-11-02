#define _DEFAULT_SOURCE
#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit
#include <sys/stat.h>     // lstat, struct stat, макросы прав
#include <unistd.h>       // стандартные вызовы
#include <pwd.h>          // getpwuid
#include <grp.h>          // getgrgid
#include <time.h>         // localtime, strftime, time
#include <string.h>       // strncpy
#include <libgen.h>       // basename



void get_perm(mode_t mode, char *perm) {
    if (S_ISDIR(mode)) {
        perm[0] = 'd';
    }
    else if (S_ISREG(mode)) {
        perm[0] = '-';
    }
    else {
        perm[0] = '?';
    }

    perm[1] = (mode & S_IRUSR) ? 'r' : '-';
    perm[2] = (mode & S_IWUSR) ? 'w' : '-';
    perm[3] = (mode & S_IXUSR) ? 'x' : '-';
    perm[4] = (mode & S_IRGRP) ? 'r' : '-';
    perm[5] = (mode & S_IWGRP) ? 'w' : '-';
    perm[6] = (mode & S_IXGRP) ? 'x' : '-';
    perm[7] = (mode & S_IROTH) ? 'r' : '-';
    perm[8] = (mode & S_IWOTH) ? 'w' : '-';
    perm[9] = (mode & S_IXOTH) ? 'x' : '-';
    perm[10] = '\0';
}

void print_file(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        perror(path);
    }

    char perm[11];
    get_perm(st.st_mode, perm);

    struct passwd *owner = getpwuid(st.st_uid);
    struct group *group = getgrgid(st.st_gid);

    const char *owner_name = owner ? owner->pw_name : "?";
    const char *group_name = group ? group->gr_name : "?";

    char date[32];

    struct tm *tm_info = localtime(&st.st_mtime);

    if (tm_info ==  NULL) {
        perror("localtime");
        return;
    }

    time_t now = time(NULL);
    double age = difftime(now, st.st_mtime);
    
    const char *format;

    if (age > 180.0 * 24 * 60 * 60) {
        format = "%b %e  %Y";
    }
    else {
        format = "%b %e %H:%M";
    }

    if (strftime(date, sizeof(date), format, tm_info) == 0) {
        fprintf(stderr, "Cannot format date: %s\n", path);
        return;
    }

    char *path_copy = strdup(path);

    if (path_copy == NULL) {
        perror("strdup");
        return;
    }

    char *name = basename(path_copy);

    if (S_ISREG(st.st_mode)) {
        printf("%-10s %3lu %-8s %-8s %8lld %s %s\n",
               perm,
               (unsigned long)st.st_nlink,
               owner_name,
               group_name,
               (long long)st.st_size,
               date,
               name);
    }
    else {
        printf("%-10s %3lu %-8s %-8s %8s %s %s\n",
               perm,
               (unsigned long)st.st_nlink,
               owner_name,
               group_name,
               "",
               date,
               name);
    }

    free(path_copy);

}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file...\n", argv[0]);
        return 1;
    }

    for (int i  = 1; i < argc; i++) {
        print_file(argv[i]);
    }

    return 0;

}