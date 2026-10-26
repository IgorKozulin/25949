CC = gcc
CFLAGS = -O2
TARGET = run_command

all: $(TARGET) crash

$(TARGET): run_command.c
	$(CC) $(CFLAGS) -o $(TARGET) run_command.c

crash: crash.c
	$(CC) -o crash crash.c

clean:
	rm -f $(TARGET) crash

.PHONY: all clean
