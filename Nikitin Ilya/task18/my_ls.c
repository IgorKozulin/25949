#include <stdio.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>

#define DATE_SIZE 32

void get_permission(mode_t mode, char *permission)
{
    if (S_ISDIR(mode))
        permission[0] = 'd';
    else if (S_ISREG(mode))
        permission[0] = '-';
    else
        permission[0] = '?';

    permission[1] = (mode & S_IRUSR) ? 'r' : '-';
    permission[2] = (mode & S_IWUSR) ? 'w' : '-';
    permission[3] = (mode & S_IXUSR) ? 'x' : '-';

    permission[4] = (mode & S_IRGRP) ? 'r' : '-';
    permission[5] = (mode & S_IWGRP) ? 'w' : '-';
    permission[6] = (mode & S_IXGRP) ? 'x' : '-';

    permission[7] = (mode & S_IROTH) ? 'r' : '-';
    permission[8] = (mode & S_IWOTH) ? 'w' : '-';
    permission[9] = (mode & S_IXOTH) ? 'x' : '-';

    permission[10] = '\0';
}

void get_date(time_t mtime, char *date)
{
    time_t now;
    struct tm *time_info;

    now = time(NULL);
    time_info = localtime(&mtime);

    if (now - mtime > 180L * 24L * 60L * 60L)
    {
        strftime(date, DATE_SIZE, "%b %e %Y", time_info);
    }
    else
    {
        strftime(date, DATE_SIZE, "%b %e %H:%M", time_info);
    }
}

int main(int argc, char *argv[])
{
    int i;
    struct stat st;
    struct passwd *user;
    struct group *group;
    char permissions[11];
    char date[DATE_SIZE];
    char *name;

    if (argc < 2)
    {
        fprintf(stderr, "Usage %s file...\n", argv[0]);
        return 1;
    }

    for (i = 1; i < argc; i++)
    {
        if (lstat(argv[i], &st) == -1)
        {
            perror(argv[i]);
            continue;
        }

        get_permission(st.st_mode, permissions);
        user = getpwuid(st.st_uid);
        group = getgrgid(st.st_gid);
        get_date(st.st_mtime, date);
        name = basename(argv[i]);

        if (S_ISREG(st.st_mode))
        {
            printf("%-10s %4ld %-8s %-8s %10ld %-15s %s\n", permissions,
                   (long)st.st_nlink, user != NULL ? user->pw_name : "?", group != NULL ? group->gr_name : "?", (long)st.st_size, date, name);
        }
        else
        {
            printf("%-10s %4ld %-8s %-8s %10s %-15s %s\n", permissions,
                   (long)st.st_nlink, user != NULL ? user->pw_name : "?", group != NULL ? group->gr_name : "?", "", date, name);
        }
    }
    return 0;
}