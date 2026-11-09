#include <stdio.h>        // printf, perror
#include <stdlib.h>       // exit, EXIT_FAILURE
#include <unistd.h>       // read, write, close, unlink
#include <string.h>       // memset, strncpy, strlen
#include <ctype.h>        // toupper
#include <sys/socket.h>   // socket, bind, listen, accept, connect
#include <sys/un.h>       // struct sockaddr_un, AF_UNIX
#include <errno.h>        // обработка ошибок

#define SOCKET_PATH "./socket"

int main() {
    int server_fd;
    int client_fd;

    struct sockaddr_un address;
    char buffer[256];
    // 1. Удаляем старый файл сокета
    unlink(SOCKET_PATH);
    // 2. Создаём сокет
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    // 3. Настраиваем адрес сокета
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

    // 4. Привязываем сокет к адресу
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 5. Начинаем прослушивать подключения
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        exit(EXIT_FAILURE);
    }

    printf("Сервер запущен. Ожидание подключения...\n");
    // 6. Принимаем подключение клиента
    client_fd = accept(server_fd, NULL, NULL);

    if(client_fd == -1) {
        perror("accept");
        close(server_fd);
        unlink(SOCKET_PATH);
        exit(EXIT_FAILURE);
    }

    printf("Клиент подключился!\n");
    // 7. Получаем и обрабатываем данные
    ssize_t bytes_read;

    while ((bytes_read = read(client_fd, buffer, sizeof(buffer))) > 0) {
        for (int i = 0; i < bytes_read; i++) {
            buffer[i] = toupper((unsigned char)buffer[i]);
        }

        write(STDOUT_FILENO, buffer, bytes_read);
    }

    if (bytes_read == -1) {
        perror("read");
    }
    printf("\nКлиент отключился. Завершение работы сервера.\n");

    // 8. Закрываем сокеты и удаляем файл
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);

    return 0;

}