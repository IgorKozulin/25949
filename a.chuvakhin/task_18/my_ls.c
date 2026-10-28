#include <stdio.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file...\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (lstat(argv[i], &st) == -1) {
            perror(argv[i]);
            continue;
        }

        char perm[11];
        perm[0] = S_ISDIR(st.st_mode) ? 'd' : S_ISREG(st.st_mode) ? '-' : '?';
        perm[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
        perm[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
        perm[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
        perm[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
        perm[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
        perm[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
        perm[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
        perm[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
        perm[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
        perm[10] = '\0';

        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);

        char date[32];
        const char *fmt = (time(NULL) - st.st_mtime > 6 * 30 * 24 * 3600)
                          ? "%b %e  %Y" : "%b %e %H:%M";
        strftime(date, sizeof(date), fmt, localtime(&st.st_mtime));

        printf("%s %4ld %-8s %-8s ", perm, (long)st.st_nlink,
               pw ? pw->pw_name : "?", gr ? gr->gr_name : "?");
        if (S_ISREG(st.st_mode))
            printf("%10lld ", (long long)st.st_size);
        else
            printf("%10s ", "");
        printf("%s %s\n", date, basename(argv[i]));
    }
    return 0;
}
