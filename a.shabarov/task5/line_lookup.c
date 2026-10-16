#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

// Структура для хранения информации об одной строке
typedef struct {
    off_t offset;  // Смещение от начала файла (в байтах)
    int length;    // Длина строки (в байтах, включая '\n')
} LineInfo;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
        return 1;
    }

    // 1. Открываем файл через системный вызов open (только для чтения)
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Ошибка при открытии файла");
        return 1;
    }

    // Динамический массив для таблицы строк
    int capacity = 10;
    int num_lines = 0;
    LineInfo *table = (LineInfo *)malloc(capacity * sizeof(LineInfo));
    if (table == NULL) {
        perror("malloc");
        close(fd);
        return 1;
    }

    // 2. Строим таблицу строк (сканируем файл)
    off_t line_start = 0;
    int line_len = 0;
    char ch;

    while (read(fd, &ch, 1) > 0) {
        line_len++;
        if (ch == '\n') {
            // Если массив заполнен — удваиваем его размер через realloc
            if (num_lines >= capacity) {
                capacity *= 2;
                LineInfo *new_table = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
                if (new_table == NULL) {
                    perror("realloc");
                    free(table);
                    close(fd);
                    return 1;
                }
                table = new_table;
            }

            // Записываем данные о строке в таблицу
            table[num_lines].offset = line_start;
            table[num_lines].length = line_len;
            num_lines++;

            // Запоминаем позицию начала следующей строки (используем подсказку: lseek с SEEK_CUR)
            line_start = lseek(fd, 0L, SEEK_CUR);
            line_len = 0;
        }
    }

    // Обработка последней строки, если файл не заканчивается переводом строки '\n'
    if (line_len > 0) {
        if (num_lines >= capacity) {
            capacity += 1;
            table = (LineInfo *)realloc(table, capacity * sizeof(LineInfo));
        }
        table[num_lines].offset = line_start;
        table[num_lines].length = line_len;
        num_lines++;
    }

    // 3. Отладочный вывод таблицы (как требует задание)
    printf("--- Таблица строк (%d строк) ---\n", num_lines);
    for (int i = 0; i < num_lines; i++) {
        printf("Строка %d: Смещение (Offset) = %ld, Длина (Length) = %d\n",
               i + 1, (long)table[i].offset, table[i].length);
    }
    printf("--------------------------------\n");

    // 4. Интерактивный цикл запроса строк
    int target_line;
    while (1) {
        printf("Введите номер строки (0 для выхода): ");
        if (scanf("%d", &target_line) != 1) {
            // Защита от некорректного ввода (буквы и т.п.)
            printf("Ошибка ввода!\n");
            break;
        }

        if (target_line == 0) {
            printf("Выход из программы.\n");
            break;
        }

        if (target_line < 1 || target_line > num_lines) {
            printf("Неверный номер строки! Допустимо: от 1 до %d\n", num_lines);
            continue;
        }

        // Получаем информацию о запрошенной строке
        LineInfo info = table[target_line - 1];

        // Прыгаем напрямую к началу строки в файле
        lseek(fd, info.offset, SEEK_SET);

        // Выделяем буфер ровно под длину строки + 1 байт под завершающий нуль '\0'
        char *buffer = (char *)malloc(info.length + 1);
        if (buffer == NULL) {
            perror("malloc buffer");
            break;
        }

        // Читаем точную длину строки
        ssize_t bytes_read = read(fd, buffer, info.length);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Обязательно добавляем завершающий нуль для printf
            printf("%s", buffer);
            // Если в конце файла не было '\n', добавим перенос
            if (buffer[bytes_read - 1] != '\n') {
                printf("\n");
            }
        }

        free(buffer);
    }

    // 5. Очистка ресурсов
    free(table);
    close(fd);

    return 0;
}
