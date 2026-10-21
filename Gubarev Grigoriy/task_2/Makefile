CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = ex_time
SRC = ex_time.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
