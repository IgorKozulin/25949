CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2

all: beeper

beeper: beeper.c
	$(CC) $(CFLAGS) -o beeper beeper.c

clean:
	rm -f beeper

.PHONY: all clean
