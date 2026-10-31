CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CFLAGS_DEBUG = -std=c11 -Wall -Wextra -Wpedantic -g -O1 \
               -fsanitize=address,undefined -fno-omit-frame-pointer
BIN     = build/ecotech
BIN_ASAN = build/ecotech-asan

.PHONY: all run bench clean asan valgrind mem

all: $(BIN)

$(BIN): src/main.c | build
	$(CC) $(CFLAGS) src/main.c -o $@

build:
	mkdir -p build

run: $(BIN)
	./$(BIN)

bench: $(BIN)
	./$(BIN) --benchmark

asan: | build
	$(CC) $(CFLAGS_DEBUG) src/main.c -o $(BIN_ASAN)
	./$(BIN_ASAN)

valgrind: $(BIN)
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./$(BIN)

clean:
	rm -rf build
