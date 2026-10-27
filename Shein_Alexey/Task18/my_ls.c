#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <string.h>
#include <libgen.h>
#include <inttypes.h>

static void format_permissions(mode_t mode, char result[11])
{
    const mode_t bits[9] = {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH
    };
    const char symbols[] = "rwxrwxrwx";
    int i;

    if (S_ISDIR(mode)) {
        result[0] = 'd';
    } else if (S_ISREG(mode)) {
        result[0] = '-';
    } else {
        result[0] = '?';
    }

    for (i = 0; i < 9; ++i) {
        result[i + 1] = (mode & bits[i]) ? symbols[i] : '-';
    }

    result[10] = '\0';
}

static int print_file(const char *path, time_t now)
{
    struct stat st;
    struct passwd *owner;
    struct group *group;
    struct tm *modified;
    char permissions[11];
    char owner_id[32];
    char group_id[32];
    char size[64] = "";
    char date[64];
    char *path_copy;
    const char *owner_name;
    const char *group_name;
    const char *date_format;
    double age;
    int result;

    if (lstat(path, &st) == -1) {
        perror(path);
        return -1;
    }

    format_permissions(st.st_mode, permissions);

    owner = getpwuid(st.st_uid);
    if (owner != NULL) {
        owner_name = owner->pw_name;
    } else {
        snprintf(owner_id, sizeof(owner_id), "%ju",
                 (uintmax_t)st.st_uid);
        owner_name = owner_id;
    }

    group = getgrgid(st.st_gid);
    if (group != NULL) {
        group_name = group->gr_name;
    } else {
        snprintf(group_id, sizeof(group_id), "%ju",
                 (uintmax_t)st.st_gid);
        group_name = group_id;
    }

    if (S_ISREG(st.st_mode)) {
        snprintf(size, sizeof(size), "%jd", (intmax_t)st.st_size);
    }

    modified = localtime(&st.st_mtime);
    if (modified == NULL) {
        fprintf(stderr, "%s: cannot convert modification time\n",
                path);
        return -1;
    }

    /* Use 183 days as an approximation of six months. */
    age = difftime(now, st.st_mtime);
    date_format = (age < 0 || age > 183.0 * 24 * 60 * 60)
                  ? "%b %e  %Y" : "%b %e %H:%M";

    if (strftime(date, sizeof(date), date_format, modified) == 0) {
        fprintf(stderr, "%s: cannot format modification time\n",
                path);
        return -1;
    }

    /* basename() may modify its argument. */
    path_copy = strdup(path);
    if (path_copy == NULL) {
        perror("strdup");
        return -1;
    }

    result = printf("%-10s %5ju %-12s %-12s %12s %-12s %s\n",
                    permissions,
                    (uintmax_t)st.st_nlink,
                    owner_name,
                    group_name,
                    size,
                    date,
                    basename(path_copy));

    free(path_copy);

    if (result < 0) {
        perror("stdout");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    time_t now;
    int i;
    int status = EXIT_SUCCESS;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s path [path...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        perror("time");
        return EXIT_FAILURE;
    }

    for (i = 1; i < argc; ++i) {
        if (print_file(argv[i], now) == -1) {
            status = EXIT_FAILURE;
        }
    }

    if (fflush(stdout) == EOF) {
        perror("stdout");
        status = EXIT_FAILURE;
    }

    return status;
}
