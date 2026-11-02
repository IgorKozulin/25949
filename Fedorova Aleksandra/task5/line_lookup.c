#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    long offset;
    long length;
} Line;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s file\n", argv[0]);
        return 1;
    }

    // открываем файл для чтения
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    Line *lines = NULL;
    int count = 0;
    int capacity = 0;

    long start = 0;
    char c;

    // читаем файл и строим таблицу строк
    while (read(fd, &c, 1) == 1) {
        if (c == '\n') {
            long pos = lseek(fd, 0L, SEEK_CUR);

            // увеличиваем таблицу, если места не хватает
            if (count == capacity) {
                capacity = capacity == 0 ? 4 : capacity * 2;
                Line *temp = realloc(lines, capacity * sizeof(Line));

                if (temp == NULL) {
                    perror("Memory allocation error");
                    free(lines);
                    close(fd);
                    return 1;
                }

                lines = temp;
            }

            lines[count].offset = start;
            lines[count].length = pos - start - 1;
            count++;

            start = pos;
        }
    }

    // добавляем последнюю строку, если нет '\n' в конце файла
    long end = lseek(fd, 0L, SEEK_CUR);

    if (end > start) {
        if (count == capacity) {
            capacity = capacity == 0 ? 4 : capacity * 2;
            Line *temp = realloc(lines, capacity * sizeof(Line));

            if (temp == NULL) {
                perror("Memory allocation error");
                free(lines);
                close(fd);
                return 1;
            }

            lines = temp;
        }

        lines[count].offset = start;
        lines[count].length = end - start;
        count++;
    }

    // выводим таблицу для проверки
    printf("Line table:\n");
    for (int i = 0; i < count; i++) {
        printf("Line %d: Length = %ld\n",
               i + 1, lines[i].length);
    }

    int number;

    while (1) {
        printf("Enter line number (0 to quit): ");
        fflush(stdout);

        if (scanf("%d", &number) != 1) {
            // не число — очищаем входной поток и выходим (или продолжаем)
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) { }
            if (feof(stdin)) break;
            printf("Invalid input\n");
            continue;
        }

        if (number == 0)
            break;

        if (number < 1 || number > count) {
            printf("Line not found\n");
            continue;
        }

        Line line = lines[number - 1];

        if (lseek(fd, line.offset, SEEK_SET) == -1) {
            perror("Error seeking in file");
            break;
        }

        char *buffer = malloc(line.length + 1);
        if (buffer == NULL) {
            perror("Memory allocation error");
            break;
        }

        long total = 0;
        while (total < line.length) {
            ssize_t n = read(fd, buffer + total, line.length - total);
            if (n <= 0) break;
            total += n;
        }

        buffer[total] = '\0';
        printf("%s\n", buffer);
        free(buffer);
    }

    // освобождаем ресурсы
    free(lines);
    close(fd);

    return 0;
}


