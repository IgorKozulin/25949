CC = gcc
CFLAGS = -O2
TARGET = options
SRC = optional.c

all: $(TARGET)

$(TARGET): $(SRC)
	 $(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	 rm -f $(TARGET)

.PHONY: all clean
