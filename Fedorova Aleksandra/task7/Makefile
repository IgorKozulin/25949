CC = gcc
CFLAGS = -O2
TARGET = line_lookup_nmap
SRC = line_lookup_nmap.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
