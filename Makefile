BASE = 16

CC = gcc
CFLAGS = -Wall -Wextra -ggdb3 -gdwarf -DBASE=$(BASE)

all:
	mkdir -p build
	$(CC) -o build/funcs $(CFLAGS) src/funcs.c

run:
	mkdir -p build
	$(CC) -o build/funcs-fast $(CFLAGS) -O3 src/funcs.c && ./build/funcs-fast

debug_gentab:
	mkdir -p build
	$(CC) -o build/testtablegen $(CFLAGS) src/testtablegen.c

gentab:
	mkdir -p build
	$(CC) -o build/testtablegen $(CFLAGS) -O3 src/testtablegen.c && ./build/testtablegen

gensine:
	mkdir -p build
	$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis
