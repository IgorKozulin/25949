#include <stdio.h>        // printf, perror, fflush
#include <stdlib.h>       // exit, EXIT_SUCCESS
#include <unistd.h>       // pipe, fork, read, write, close
#include <string.h>       // strlen, strcspn
#include <ctype.h>        // toupper
#include <sys/wait.h>     // wait

int main() {
    int pipefd[2]; //массив из двух файлов дескрипторов
    pid_t pid; //айдишник процесса
    char buffer[256]; //временное хранение текста

    if (pipe(pipefd) == -1) { //принимает массив из двух элементов и записывает в него файловые дескрипторы.
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid == -1) { //ошибка создания процесса
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { //проверка в дочеренем ли мы процессе
        close(pipefd[1]); //закрываем конец записи
        printf("[CHILD] Ожидаю данные...\n");
        fflush(stdout); //очистка буфера вывода
        ssize_t bytes_read;
        while((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) { //чтение данных из канала
            for (int i = 0; i < bytes_read; i++) { //перевод в верхний регистр
                buffer[i] = toupper((unsigned char)buffer[i]);
            }
            fwrite(buffer, 1, bytes_read, stdout);
            fflush(stdout);
        }

        if (bytes_read == -1) {
            perror("read");
        }

        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    }

    else {
        close(pipefd[0]);
        printf("[PARENT] Введите текст (пустая строка для выхода):\n");
        while (fgets(buffer, sizeof(buffer), stdin) != NULL) { //читаем текст с клавиатуры
            if (buffer[0] == '\n') {  //условие выхода
                break;
            }
            size_t length = strlen(buffer); //запись данных в канал
            size_t total_written = 0;

            while (total_written < length) { //отправка данных в канал
                //адрес, куда записываем и сколько байт осталось
                ssize_t bytes_written = write(pipefd[1], buffer + total_written, length - total_written);
                if (bytes_written == -1) {
                    perror("write");
                    break;
                }
                total_written += bytes_written;
            }
            if (total_written < length) {
                break;
            }
        }
        close(pipefd[1]);
        waitpid(pid, NULL, 0); //ожидание заверешения потомка
        printf("\n[PARENT] Работа завершена.\n");
    }

    return 0;

}