#define _POSIX_C_SOURCE 200809L
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* A file older than about six months shows its year instead of the time. */
#define HALF_YEAR_SECONDS (183L * 24L * 60L * 60L)

/* "d" directory, "-" regular file, "?" anything else, then rwx for owner, group, others. */
static void format_mode(mode_t mode, char out[11])
{
    out[0] = S_ISDIR(mode) ? 'd' : S_ISREG(mode) ? '-' : '?';
    out[1] = (mode & S_IRUSR) ? 'r' : '-';
    out[2] = (mode & S_IWUSR) ? 'w' : '-';
    out[3] = (mode & S_IXUSR) ? 'x' : '-';
    out[4] = (mode & S_IRGRP) ? 'r' : '-';
    out[5] = (mode & S_IWGRP) ? 'w' : '-';
    out[6] = (mode & S_IXGRP) ? 'x' : '-';
    out[7] = (mode & S_IROTH) ? 'r' : '-';
    out[8] = (mode & S_IWOTH) ? 'w' : '-';
    out[9] = (mode & S_IXOTH) ? 'x' : '-';
    out[10] = '\0';
}

static void format_owner(uid_t uid, char *out, size_t size)
{
    const struct passwd *user = getpwuid(uid);

    if (user != NULL)
        snprintf(out, size, "%s", user->pw_name);
    else
        snprintf(out, size, "%lu", (unsigned long)uid); /* unknown user: show the number, like ls */
}

static void format_group(gid_t gid, char *out, size_t size)
{
    const struct group *group = getgrgid(gid);

    if (group != NULL)
        snprintf(out, size, "%s", group->gr_name);
    else
        snprintf(out, size, "%lu", (unsigned long)gid);
}

/* Both variants are 12 characters wide, so the date column stays aligned. */
static void format_time(time_t mtime, char *out, size_t size)
{
    time_t now = time(NULL);
    const struct tm *local = localtime(&mtime);
    int show_year = mtime < now - HALF_YEAR_SECONDS || mtime > now;

    if (local == NULL || strftime(out, size, show_year ? "%b %e  %Y" : "%b %e %H:%M", local) == 0)
        snprintf(out, size, "?");
}

static void print_entry(const char *path, const struct stat *info)
{
    char mode[11];
    char owner[64];
    char group[64];
    char size[32] = ""; /* stays empty for everything except regular files */
    char date[32];
    char copy[PATH_MAX];

    format_mode(info->st_mode, mode);
    format_owner(info->st_uid, owner, sizeof(owner));
    format_group(info->st_gid, group, sizeof(group));
    if (S_ISREG(info->st_mode))
        snprintf(size, sizeof(size), "%lld", (long long)info->st_size);
    format_time(info->st_mtime, date, sizeof(date));

    /* basename() may modify its argument, so give it a copy of the path. */
    strncpy(copy, path, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    printf("%s %3ld %-8s %-8s %10s %s %s\n", mode, (long)info->st_nlink,
           owner, group, size, date, basename(copy));
}

int main(int argc, char **argv)
{
    int failed = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file> [file ...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; ++i) {
        struct stat info;

        /* lstat() describes a symbolic link itself instead of the file it points to. */
        if (lstat(argv[i], &info) == -1) {
            perror(argv[i]);
            failed = 1;
            continue;
        }
        print_entry(argv[i], &info);
    }
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
