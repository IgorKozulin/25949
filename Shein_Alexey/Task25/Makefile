CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2

all: pipe_upper

pipe_upper: pipe_upper.c
	$(CC) $(CFLAGS) -o pipe_upper pipe_upper.c

clean:
	rm -f pipe_upper

.PHONY: all clean
