CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200112L

all: server client

server: server.c
	$(CC) $(CFLAGS) -o $@ $@.c $(LDLIBS)

client: client.c
	$(CC) $(CFLAGS) -o $@ $@.c $(LDLIBS)

clean:
	rm -f server client

.PHONY: all clean
