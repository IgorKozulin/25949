#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

typedef struct {
    long offset;
    long length;
} Line;

int timeout_fd;

// обработчик таймера
void alarm_handler(int sig) {
    char buffer[256];
    ssize_t n;

    (void)sig;

    write(STDOUT_FILENO, "\nTime is up! Printing full file content\n", 44);

    lseek(timeout_fd, 0, SEEK_SET);

    while ((n = read(timeout_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t written = 0;

        while (written < n) {
            ssize_t w = write(STDOUT_FILENO, buffer + written, n - written);
            if (w <= 0)
                _exit(1);

            written += w;
        }
    }

    close(timeout_fd);
    _exit(0);
}

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

    // открываем файл отдельно для обработчика таймера
    timeout_fd = open(argv[1], O_RDONLY);
    if (timeout_fd == -1) {
        perror("Error opening file");
        close(fd);
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

            if (count == capacity) {
                capacity = capacity == 0 ? 4 : capacity * 2;
                Line *temp = realloc(lines, capacity * sizeof(Line));

                if (temp == NULL) {
                    perror("Memory allocation error");
                    free(lines);
                    close(fd);
                    close(timeout_fd);
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
                close(timeout_fd);
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
        printf("Line %d: Offset = %ld, Length = %ld\n",
               i + 1, lines[i].offset, lines[i].length);
    }

    // устанавливаем обработчик сигнала
    signal(SIGALRM, alarm_handler);

    int number;

    // запрашиваем номер строки с таймером
    while (1) {
        printf("Enter line number (0 to quit, 5 sec timeout): ");
        fflush(stdout);

        alarm(5);

        if (scanf("%d", &number) != 1) {
            alarm(0);
            printf("Invalid input\n");
            continue;
        }

        // отменяем таймер после ввода
        alarm(0);

        if (number == 0)
            break;

        if (number < 1 || number > count) {
            printf("Line not found\n");
            continue;
        }

        // переходим к началу нужной строки
        Line line = lines[number - 1];

        if (lseek(fd, line.offset, SEEK_SET) == -1) {
            perror("Error seeking in file");
            break;
        }

        // читаем строку и добавляем конец строки
        char *buffer = malloc(line.length + 1);

        if (buffer == NULL) {
            perror("Memory allocation error");
            break;
        }

        long total = 0;

        while (total < line.length) {
            ssize_t n = read(fd, buffer + total, line.length - total);

            if (n <= 0)
                break;

            total += n;
        }

        buffer[total] = '\0';
        printf("%s\n", buffer);

        free(buffer);
    }

    free(lines);
    close(fd);
    close(timeout_fd);

    return 0;
}

