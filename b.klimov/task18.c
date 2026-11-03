/* task18.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>

void print_file_info(const char *path) {
    struct stat st;

    /* lstat не разыменовывает симлинки, как и положено утилите ls -d */
    if (lstat(path, &st) == -1) {
        perror(path);
        return;
    }

    /* 1. Определение типа файла: 'd', '-' или '?' */
    char file_type;
    if (S_ISDIR(st.st_mode)) {
        file_type = 'd';
    } else if (S_ISREG(st.st_mode)) {
        file_type = '-';
    } else {
        file_type = '?';
    }

    /* 2. Формирование прав доступа */
    char permissions[10];
    permissions[0] = (st.st_mode & S_IRUSR) ? 'r' : '-';
    permissions[1] = (st.st_mode & S_IWUSR) ? 'w' : '-';
    permissions[2] = (st.st_mode & S_IXUSR) ? 'x' : '-';
    permissions[3] = (st.st_mode & S_IRGRP) ? 'r' : '-';
    permissions[4] = (st.st_mode & S_IWGRP) ? 'w' : '-';
    permissions[5] = (st.st_mode & S_IXGRP) ? 'x' : '-';
    permissions[6] = (st.st_mode & S_IROTH) ? 'r' : '-';
    permissions[7] = (st.st_mode & S_IWOTH) ? 'w' : '-';
    permissions[8] = (st.st_mode & S_IXOTH) ? 'x' : '-';
    permissions[9] = '\0';

    /* 3. Количество жестких связей */
    long nlink = (long)st.st_nlink;

    /* 4. Имя владельца */
    char owner_name[64];
    struct passwd *pw = getpwuid(st.st_uid);
    if (pw != NULL) {
        snprintf(owner_name, sizeof(owner_name), "%s", pw->pw_name);
    } else {
        snprintf(owner_name, sizeof(owner_name), "%u", (unsigned int)st.st_uid);
    }

    /* 5. Имя группы */
    char group_name[64];
    struct group *gr = getgrgid(st.st_gid);
    if (gr != NULL) {
        snprintf(group_name, sizeof(group_name), "%s", gr->gr_name);
    } else {
        snprintf(group_name, sizeof(group_name), "%u", (unsigned int)st.st_gid);
    }

    /* 6. Размер файла (печатается только для обычных файлов) */
    char size_str[32];
    if (S_ISREG(st.st_mode)) {
        snprintf(size_str, sizeof(size_str), "%lld", (long long)st.st_size);
    } else {
        size_str[0] = '\0'; /* Оставляем поле пустым */
    }

    /* 7. Дата модификации (mtime) */
    char time_str[32];
    struct tm *tm_info = localtime(&st.st_mtime);
    if (tm_info != NULL) {
        strftime(time_str, sizeof(time_str), "%b %e %H:%M", tm_info);
    } else {
        snprintf(time_str, sizeof(time_str), "???");
    }

    /* 8. Только имя файла (без полного пути) */
    char path_copy[1024];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    char *base_name = basename(path_copy);

    /* Вывод в формате таблицы с фиксированной шириной столбцов */
    printf("%c%s %3ld %-10s %-10s %10s %-12s %s\n",
           file_type,
           permissions,
           nlink,
           owner_name,
           group_name,
           size_str,
           time_str,
           base_name);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        /* Если аргументы не переданы, выводим текущий каталог "." */
        print_file_info(".");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        print_file_info(argv[i]);
    }

    return 0;
}

/*
gcc -Wall -Wextra task18.c -o task18


# 1. Обычный текстовый файл с содержимым (для проверки типа '-' и вывода размера)
echo "Тестовый контент для проверки размера файла" > test_regular.txt

# 2. Исполняемый файл (для проверки флагов прав rwx)
touch test_exec.sh
chmod 755 test_exec.sh

# 3. Каталог (для проверки типа 'd' и пустого поля размера)
mkdir test_directory

# 4. Символическая ссылка (для проверки типа '?')
ln -s test_regular.txt test_symlink

# 5. Именованный канал / пайп (FIFO) (еще один тип '?')
mkfifo test_pipe

./task18 test_directory test_regular.txt test_exec.sh test_symlink test_pipe
ls -ld test_directory test_regular.txt test_exec.sh test_symlink test_pipe
*/