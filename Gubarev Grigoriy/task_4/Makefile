CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = list_strings
SRC = list_strings.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
