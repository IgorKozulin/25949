CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = setuid_test

all: $(TARGET)

$(TARGET): setuid_test.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
