CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET = line_editor
SRC = line_editor.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
