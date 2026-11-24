#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit
#include <sys/stat.h>     // lstat, struct stat, макросы прав
#include <unistd.h>       // стандартные вызовы
#include <pwd.h>          // getpwuid
#include <grp.h>          // getgrgid
#include <time.h>         // localtime, strftime, time
#include <string.h>       // strncpy
#include <libgen.h>       // basename

void format_permissions(mode_t mode, char *buf)
{
    switch (mode & S_IFMT)
    {
        case S_IFDIR:  buf[0] = 'd'; break;
        case S_IFREG:  buf[0] = '-'; break;
        case S_IFLNK:  buf[0] = 'l'; break;
        case S_IFCHR:  buf[0] = 'c'; break;
        case S_IFBLK:  buf[0] = 'b'; break;
        case S_IFIFO:  buf[0] = 'p'; break;
        case S_IFSOCK: buf[0] = 's'; break;
        default:       buf[0] = '?'; break;
    }

    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';

    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';

    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';

    buf[10] = '\0';
}

void format_date(time_t mtime, char *buf, size_t bufsize)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&mtime);

    
    if (now - mtime > 15552000L)
    {
        strftime(buf, bufsize, "%b %e  %Y", tm_info);
    } else
    {
        strftime(buf, bufsize, "%b %e %H:%M", tm_info);
    }
}


int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Не получилось с файлом %s\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        struct stat st;
        if (lstat(argv[i], &st) == -1)
        {
            perror("argv[i]");
            continue;
        }
        char perms[11];
        format_permissions(st.st_mode, perms);

        struct passwd *pw = getpwuid(st.st_uid);
        const char *owner = pw ? pw->pw_name : "?";

        struct group *gr = getgrgid(st.st_gid);
        const char *group = gr ? gr->gr_name : "?";

        char size_str[32];
        if (S_ISREG(st.st_mode))
        {
            snprintf(size_str, sizeof(size_str), "%lld", (long long)st.st_size);
        } else {
            size_str[0] = '\0'; 
        }

        char date_str[64];
        format_date(st.st_mtime, date_str, sizeof(date_str));

        char path_copy[4096];
        strncpy(path_copy, argv[i], sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';
        char *name = basename(path_copy);

        printf("%s %3ld %-8s %-8s %10s %s %s\n",
               perms,
               (long)st.st_nlink, owner, group, size_str, date_str, name);
    }
    return 0;
}
