CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
LDLIBS = -lsocket -lnsl
all: server client
server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDLIBS)
client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDLIBS)
