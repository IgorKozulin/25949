#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    long offset;
    long length;
} Line;

char *file_map;
long file_size;

// обработчик таймера
void alarm_handler(int sig) {
    (void)sig;

    write(STDOUT_FILENO,
          "\nTime is up! Printing full file content...\n",
          43);

    // выводим всё содержимое отображённого файла
    write(STDOUT_FILENO, file_map, file_size);

    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s file\n", argv[0]);
        return 1;
    }

    // открываем файл
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    // получаем размер файла
    struct stat st;

    if (fstat(fd, &st) == -1) {
        perror("Error getting file information");
        close(fd);
        return 1;
    }

    file_size = st.st_size;

    // проверяем, пустой ли файл
    if (file_size == 0) {
        printf("File is empty.\n");
        close(fd);
        return 0;
    }

    // отображаем файл в память
    file_map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (file_map == MAP_FAILED) {
        perror("Error mapping file");
        close(fd);
        return 1;
    }

    close(fd);

    Line *lines = NULL;
    int count = 0;
    int capacity = 0;

    long start = 0;

    // строим таблицу строк по отображённой памяти
    for (long i = 0; i < file_size; i++) {
        if (file_map[i] == '\n') {
            if (count == capacity) {
                capacity = capacity == 0 ? 4 : capacity * 2;
                Line *temp = realloc(lines, capacity * sizeof(Line));

                if (temp == NULL) {
                    perror("Memory allocation error");
                    free(lines);
                    munmap(file_map, file_size);
                    return 1;
                }

                lines = temp;
            }

            lines[count].offset = start;
            lines[count].length = i - start;
            count++;

            start = i + 1;
        }
    }

    // добавляем последнюю строку, если нет '\n' в конце файла
    if (start < file_size) {
        if (count == capacity) {
            capacity = capacity == 0 ? 4 : capacity * 2;
            Line *temp = realloc(lines, capacity * sizeof(Line));

            if (temp == NULL) {
                perror("Memory allocation error");
                free(lines);
                munmap(file_map, file_size);
                return 1;
            }

            lines = temp;
        }

        lines[count].offset = start;
        lines[count].length = file_size - start;
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

            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;

            if (ch == EOF)
                break;

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

        // выводим строку напрямую из памяти
        Line line = lines[number - 1];

        fwrite(file_map + line.offset, 1, line.length, stdout);
        printf("\n");
    }

    // освобождаем ресурсы
    free(lines);
    munmap(file_map, file_size);

    return 0;
}

