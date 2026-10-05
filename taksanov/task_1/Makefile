CC = gcc
CFLAGS = -O2 -std=gnu99 -Wall
TARGET = options
SRC = options.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
