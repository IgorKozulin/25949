#include <stdio.h>        // printf, fprintf
#include <stdlib.h>       // exit
#include <sys/stat.h>     // lstat, struct stat, макросы прав
#include <unistd.h>       // стандартные вызовы
#include <pwd.h>          // getpwuid
#include <grp.h>          // getgrgid
#include <time.h>         // localtime, strftime, time
#include <string.h>       // strncpy
#include <libgen.h>       // basename
#include <errno.h>
#include <sys/types.h>

//Формирование строки прав доступа: 10 символов
static void mode_string(mode_t mode, char *str)
{
    //Тип файла
    if (S_ISDIR(mode))       str[0] = 'd';
    else if (S_ISREG(mode))  str[0] = '-';
    else                    str[0] = '?';

    //Права владельца
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';

    //Права группы
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';

    //Права остальных
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';

    str[10] = '\0';
}

/* Форматирование даты модификации как в ls:
    если файл старше 6 месяцев - "%b %e %Y"
    иначе - "%b %e %H:%M" 
*/
static void format_mtime(time_t mtime, char *buf, size_t bufsz)
{
    struct tm *tm;
    time_t now = time(NULL);
    const double SIX_MONTHS = 15552000.0;

    tm = localtime(&mtime);
    if (tm == NULL) {
        snprintf(buf, bufsz, "?");
        return;
    }

    if (difftime(now, mtime) > SIX_MONTHS || difftime(mtime, now) > SIX_MONTHS) {
        /* Старый файл или из будущего — показываем год */
        strftime(buf, bufsz, "%b %e  %Y", tm);
    } else {
        strftime(buf, bufsz, "%b %e %H:%M", tm);
    }
}

int main(int argc, char *argv[])
{
    int i;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s file...\n", argv[0]);
        return EXIT_FAILURE;
    }

    for (i = 1; i < argc; i++) {
        struct stat st;
        char modestr[11];
        char timebuf[64];
        const char *user = "?";
        const char *group = "?";
        struct passwd *pw;
        struct group *gr;
        char *name_copy;
        char *base;
        int is_regular;

        if (lstat(argv[i], &st) == -1) {
            fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
            continue;
        }

        //Строка прав доступа
        mode_string(st.st_mode, modestr);

        //Имена владельца и группы
        pw = getpwuid(st.st_uid);
        if (pw != NULL && pw->pw_name != NULL)
            user = pw->pw_name;

        gr = getgrgid(st.st_gid);
        if (gr != NULL && gr->gr_name != NULL)
            group = gr->gr_name;

        /* Дата модификации.
        В Solaris st_mtime - макрос, разворачивающийся в st_mtim.tv_sec,
        поэтому передаём его как time_t напрямую. 
        */
        format_mtime(st.st_mtime, timebuf, sizeof(timebuf));

        //basename() может модифицировать строку - работаем с копией
        name_copy = strdup(argv[i]);
        if (name_copy == NULL) {
            fprintf(stderr, "%s: out of memory\n", argv[0]);
            continue;
        }
        base = basename(name_copy);

        //Размер выводим только для обычных файлов
        is_regular = S_ISREG(st.st_mode);

        //Табличный вывод с фиксированной шириной полей
        if (is_regular) {
            printf("%s %4ld %-8s %-8s %8lld %s %s\n",
                   modestr,
                   (long)st.st_nlink,
                   user,
                   group,
                   (long long)st.st_size,
                   timebuf,
                   base);
        } 
        else {
            //Поле размера пустое - сохраняем ширину 8 для выравнивания
            printf("%s %4ld %-8s %-8s %8s %s %s\n",
                   modestr,
                   (long)st.st_nlink,
                   user,
                   group,
                   "",
                   timebuf,
                   base);
        }

        free(name_copy);
    }

    return EXIT_SUCCESS;
}