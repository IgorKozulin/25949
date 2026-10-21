CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = line_lookup

all: $(TARGET)

$(TARGET): line_lookup.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
