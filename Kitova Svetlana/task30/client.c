#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <signal.h>

#define SOCKET_PATH "./socket"

int main() {
    int sock_fd;
    struct sockaddr_un address;
    const char *messages[] = {
        "Hello World!\n",
        "This is a Test Message!!!\n",
        "Unix domain sockets are awesome.\n"
    };

    // 1. Игнорируем сигнал SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    // 2. Создаём сокет
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 3. Настраиваем адрес сервера
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);
    // 4. Подключаемся к серверу
    if (connect(sock_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("connect");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Подключено к серверу. Отправка сообщений...\n");

    // 5. Отправляем сообщения
    for (int i = 0; i < 3; i++) {
        printf("[CLIENT] Отправляю: %s", messages[i]);
        size_t lenght = strlen(messages[i]);
        size_t total_written = 0;

        while (total_written < lenght) {
            ssize_t bytes_written = write(sock_fd, messages[i] + total_written, lenght - total_written);
            if (bytes_written == -1) {
                perror("write");
                close(sock_fd);
                return EXIT_FAILURE;
            }
            total_written += bytes_written;
        }
    }

    printf("[CLIENT] Все сообщения отправлены. Закрытие соединения...\n");

    // 6. Закрываем соединение
    close(sock_fd);

    return 0;   


}