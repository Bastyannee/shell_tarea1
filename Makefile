CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -Iinclude -D_POSIX_C_SOURCE=200809L
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
HDR = $(wildcard include/*.h)
TARGET = mishell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Los .o dependen de los headers: si cambia un .h se recompila todo lo afectado
src/%.o: src/%.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
