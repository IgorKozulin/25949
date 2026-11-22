CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L
LDLIBS = $$(if [ "$$(uname -s)" = SunOS ]; then printf '%s' '-lsocket -lnsl'; fi) -lrt

all: server client
server: server.c Makefile
	$(CC) $(CFLAGS) -o $@ server.c $(LDLIBS)
client: client.c Makefile
	$(CC) $(CFLAGS) -o $@ client.c $(LDLIBS)
clean:
	rm -f server client
.PHONY: all clean
