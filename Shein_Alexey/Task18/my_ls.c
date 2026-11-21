#define _XOPEN_SOURCE 600

#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <inttypes.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static void make_mode_string(mode_t mode, char text[11])
{
    const mode_t masks[] = {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH
    };
    const char letters[] = "rwxrwxrwx";
    size_t i;

    if (S_ISDIR(mode)) {
        text[0] = 'd';
    } else if (S_ISREG(mode)) {
        text[0] = '-';
    } else {
        text[0] = '?';
    }

    for (i = 0; i < 9; ++i) {
        text[i + 1] = (mode & masks[i]) ? letters[i] : '-';
    }

    text[10] = '\0';
}

static const char *format_mtime(time_t mtime, time_t now,
                               char *date, size_t capacity)
{
    struct tm *local = localtime(&mtime);
    double age = difftime(now, mtime);
    const char *format;

    if (local == NULL) {
        return "?";
    }

    if (age < 0 || age > 183.0 * 24 * 60 * 60) {
        format = "%b %e  %Y";
    } else {
        format = "%b %e %H:%M";
    }

    return strftime(date, capacity, format, local) != 0 ? date : "?";
}

static int print_entry(char *path, time_t now)
{
    struct stat info;
    struct passwd *owner;
    struct group *group;
    char mode[11];
    char size[32] = "";
    char date[32];
    const char *date_text;
    const char *name;

    if (lstat(path, &info) == -1) {
        perror(path);
        return -1;
    }

    make_mode_string(info.st_mode, mode);

    if (S_ISREG(info.st_mode) || S_ISDIR(info.st_mode)) {
        snprintf(size, sizeof(size), "%jd", (intmax_t)info.st_size);
    }

    date_text = format_mtime(info.st_mtime, now, date, sizeof(date));
    owner = getpwuid(info.st_uid);
    group = getgrgid(info.st_gid);
    name = basename(path);

    if (printf("%-10s %5ju %-12s %-12s %12s %-12s %s\n",
               mode, (uintmax_t)info.st_nlink,
               owner != NULL ? owner->pw_name : "?",
               group != NULL ? group->gr_name : "?",
               size, date_text, name) < 0) {
        perror("stdout");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int i;
    int status = EXIT_SUCCESS;
    time_t now;

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
        if (print_entry(argv[i], now) == -1) {
            status = EXIT_FAILURE;
        }
    }

    if (fflush(stdout) == EOF) {
        perror("stdout");
        return EXIT_FAILURE;
    }

    return status;
}
