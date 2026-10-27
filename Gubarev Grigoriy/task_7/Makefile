CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = line_lookup_mmap
SRC = line_lookup_mmap.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
