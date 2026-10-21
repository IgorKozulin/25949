CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = ex_time

all: $(TARGET)

$(TARGET): ex_time.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
