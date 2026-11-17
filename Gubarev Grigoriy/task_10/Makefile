CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
TARGET = run_command
SRC = run_command.c

all: $(TARGET) crash_program

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

crash_program: crash_program.c
	$(CC) $(CFLAGS) -o crash_program crash_program.c

clean:
	rm -f $(TARGET) crash_program

.PHONY: all clean
