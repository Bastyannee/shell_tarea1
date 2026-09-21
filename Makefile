CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude -D_POSIX_C_SOURCE=200809L
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = mishell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
