CC = gcc
CFLAGS = -O2
TARGET = beeper
SRC = beeper.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean

Команды для запуска:
make clean
make
./beeper