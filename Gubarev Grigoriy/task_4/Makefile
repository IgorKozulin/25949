CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = list_strings

all: $(TARGET)

$(TARGET): list_strings.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
