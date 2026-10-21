CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = options

all: $(TARGET)

$(TARGET): optional.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
