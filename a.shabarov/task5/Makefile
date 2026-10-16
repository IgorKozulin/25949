CC = gcc
CFLAGS = -O2
TARGET = line_lookup
SRC = line_lookup.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
