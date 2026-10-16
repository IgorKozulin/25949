#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>

// Глобальные переменные для доступа из обработчика сигнала
char *g_map = NULL;
size_t g_file_size = 0;

typedef struct {
    off_t offset;
    int length;
} LineInfo;

// Обработчик сигнала SIGALRM (срабатывает через 5 секунд)
void alarm_handler(int sig) {
    (void)sig;

    const char msg[] = "\n[ТАЙМАУТ] Время вышло (5 сек)! Вывод всего файла через mmap:\n--------------------------------------------------\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    // Выводим всю отображенную память одним быстрым вызовом write
    if (g_map != NULL && g_file_size > 0) {
        write(STDOUT_FILENO, g_map, g_file_size);
    }

    const char finish[] = "--------------------------------------------------\nЗавершение программы по таймауту.\n";
    write(STDOUT_FILENO, finish, sizeof(finish) - 1);

    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
        return 1;
    }

    // 1. Открываем файл
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Ошибка при открытии файла");
        return 1;
    }

    // 2. Узнаем точный размер файла с помощью fstat
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("Ошибка fstat");
        close(fd);
        return 1;
    }

    // Проверка на пустой файл (mmap не умеет отображать 0 байт)
    if (st.st_size == 0) {
        printf("Файл пуст (File is empty).\n");
        close(fd);
        return 0;
    }

    g_file_size = (size_t)st.st_size;

    // 3. Отображаем файл в память (mmap)
    g_map = (char *)mmap(NULL, g_file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (g_map == MAP_FAILED) {
        perror("Ошибка mmap");
        close(fd);
        return 1;
    }

    // Файловый дескриптор можно сразу закрыть! Отображение в памяти останется жить
    close(fd);

    // Регистрируем сигнал таймаута
    signal(SIGALRM, alarm_handler);

    // 4. Сканируем файл прямо по указателю памяти (без всяких read и lseek!)
    int capacity = 10;
    int num_lines = 0;
    LineInfo *table = (LineInfo *)malloc(capacity * sizeof(LineInfo));
    if (table == NULL) {
        perror("malloc");
        munmap(g_map, g_file_size);
        return 1;
    }

    off_t line_start = 0;
    for (size_t i = 0; i < g_file_size; i++) {
        if (g_map[i] == '\n') {
            if (num_lines >= capacity) {
                capacity *= 2;
                LineInfo *new_table = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
                if (new_table == NULL) {
                    perror("realloc");
                    free(table);
                    munmap(g_map, g_file_size);
                    return 1;
                }
                table = new_table;
            }

            table[num_lines].offset = line_start;
            table[num_lines].length = (int)(i - line_start + 1); // длина с учетом '\n'
            num_lines++;

            line_start = (off_t)(i + 1);
        }
    }

    // Если в конце файла не было '\n'
    if (line_start < (off_t)g_file_size) {
        if (num_lines >= capacity) {
            capacity += 1;
            table = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
        }
        table[num_lines].offset = line_start;
        table[num_lines].length = (int)(g_file_size - line_start);
        num_lines++;
    }

    // Отладочный вывод таблицы
    printf("--- Таблица строк (%d строк) ---\n", num_lines);
    for (int i = 0; i < num_lines; i++) {
        printf("Строка %d: Смещение = %ld, Длина = %d\n",
               i + 1, (long)table[i].offset, table[i].length);
    }
    printf("--------------------------------\n");

    // 5. Интерактивный цикл с таймером
    int target_line;
    while (1) {
        printf("Введите номер строки (0 для выхода, 5 сек таймаут): ");
        fflush(stdout);

        alarm(5);
        int res = scanf("%d", &target_line);
        alarm(0); // Сброс таймера

        if (res != 1) {
            printf("Ошибка ввода!\n");
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }

        if (target_line == 0) {
            printf("Выход из программы.\n");
            break;
        }

        if (target_line < 1 || target_line > num_lines) {
            printf("Неверный номер строки! Допустимо: от 1 до %d\n", num_lines);
            continue;
        }

        LineInfo info = table[target_line - 1];

        // Выводим строку напрямую из памяти через fwrite без лишних копирований
        fwrite(g_map + info.offset, 1, info.length, stdout);
        if (g_map[info.offset + info.length - 1] != '\n') {
            printf("\n");
        }
    }

    // 6. Освобождение ресурсов
    free(table);
    munmap(g_map, g_file_size);

    return 0;
}
