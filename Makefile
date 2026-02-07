BASE = 10

CC = gcc
CFLAGS = -Wall -Wextra -ggdb3 -gdwarf -DBASE=$(BASE)

all:
	mkdir -p build
	$(CC) -o build/funcs $(CFLAGS) funcs.c

run:
	mkdir -p build
	$(CC) -o build/funcs-fast $(CFLAGS) -O3 funcs.c && ./build/funcs-fast

debug_gentab:
	mkdir -p build
	$(CC) -o build/testtablegen $(CFLAGS) testtablegen.c

gentab:
	mkdir -p build
	$(CC) -o build/testtablegen $(CFLAGS) -O3 testtablegen.c && ./build/testtablegen

