#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

// Глобальный файловый дескриптор, чтобы обработчик сигнала имел к нему доступ
int g_fd = -1;

typedef struct {
    off_t offset;
    int length;
} LineInfo;

// Обработчик сигнала SIGALRM (срабатывает через 5 секунд)
void alarm_handler(int sig) {
    (void)sig; // Подавляем предупреждение о неиспользуемом аргументе

    const char msg[] = "\n[ТАЙМАУТ] 5 секунд истекли! Вывод всего содержимого файла:\n--------------------------------------------------\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    if (g_fd != -1) {
        // Перемещаемся в самое начало файла
        lseek(g_fd, 0, SEEK_SET);

        char buf[512];
        ssize_t bytes;
        // Читаем и сразу выводим весь файл на экран через низкоуровневый write
        while ((bytes = read(g_fd, buf, sizeof(buf))) > 0) {
            write(STDOUT_FILENO, buf, bytes);
        }
    }

    const char finish[] = "--------------------------------------------------\nЗавершение программы по таймауту.\n";
    write(STDOUT_FILENO, finish, sizeof(finish) - 1);

    // Безопасное немедленное завершение процесса внутри сигнала
    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
        return 1;
    }

    g_fd = open(argv[1], O_RDONLY);
    if (g_fd == -1) {
        perror("Ошибка при открытии файла");
        return 1;
    }

    // Регистрируем наш обработчик для сигнала SIGALRM
    signal(SIGALRM, alarm_handler);

    // 1. Построение таблицы смещений строк (как в задаче 5)
    int capacity = 10;
    int num_lines = 0;
    LineInfo *table = (LineInfo *)malloc(capacity * sizeof(LineInfo));
    if (table == NULL) {
        perror("malloc");
        close(g_fd);
        return 1;
    }

    off_t line_start = 0;
    int line_len = 0;
    char ch;

    while (read(g_fd, &ch, 1) > 0) {
        line_len++;
        if (ch == '\n') {
            if (num_lines >= capacity) {
                capacity *= 2;
                LineInfo *new_table = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
                if (new_table == NULL) {
                    perror("realloc");
                    free(table);
                    close(g_fd);
                    return 1;
                }
                table = new_table;
            }

            table[num_lines].offset = line_start;
            table[num_lines].length = line_len;
            num_lines++;

            line_start = lseek(g_fd, 0L, SEEK_CUR);
            line_len = 0;
        }
    }

    if (line_len > 0) {
        if (num_lines >= capacity) {
            capacity += 1;
            table = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
        }
        table[num_lines].offset = line_start;
        table[num_lines].length = line_len;
        num_lines++;
    }

    // Отладочный вывод таблицы
    printf("--- Таблица строк (%d строк) ---\n", num_lines);
    for (int i = 0; i < num_lines; i++) {
        printf("Строка %d: Смещение = %ld, Длина = %d\n",
               i + 1, (long)table[i].offset, table[i].length);
    }
    printf("--------------------------------\n");

    // 2. Интерактивный цикл с таймером на 5 секунд
    int target_line;
    while (1) {
        printf("Введите номер строки (0 для выхода, 5 сек таймаут): ");
        // Обязательно сбрасываем буфер stdout, чтобы текст сразу появился на экране!
        fflush(stdout);

        // Взводим таймер ядра на 5 секунд
        alarm(5);

        int res = scanf("%d", &target_line);

        // Если пользователь успел нажать Enter — СРОЧНО отключаем таймер!
        alarm(0);

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

        // Выводим строку по смещению
        LineInfo info = table[target_line - 1];
        lseek(g_fd, info.offset, SEEK_SET);

        char *buffer = (char *)malloc(info.length + 1);
        if (buffer == NULL) {
            perror("malloc buffer");
            break;
        }

        ssize_t bytes_read = read(g_fd, buffer, info.length);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
            if (buffer[bytes_read - 1] != '\n') {
                printf("\n");
            }
        }

        free(buffer);
    }

    free(table);
    close(g_fd);
    return 0;
}
