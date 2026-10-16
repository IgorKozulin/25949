CC = gcc
CFLAGS = -O2 -Wall -Wextra
TARGET = run_command
SRC = run_command.c
CRASH = crash_program

all: $(TARGET) $(CRASH)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

$(CRASH): crash_program.c
	$(CC) -O0 -o $(CRASH) crash_program.c

clean:
	rm -f $(TARGET) $(CRASH)

.PHONY: all clean
