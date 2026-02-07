BASE = 11

all:
	mkdir -p build
	gcc -o build/funcs -Wall -Wextra -ggdb3 -gdwarf funcs.c

run:
	gcc -o build/funcs-fast -Wall -Wextra -O3 funcs.c && ./build/funcs-fast
	mkdir -p build

debug_gentab:
	mkdir -p build
	gcc -o build/testtablegen -Wall -Wextra -ggdb3 -gdwarf -DBASE=$(BASE) testtablegen.c

gentab:
	mkdir -p build
	gcc -o build/testtablegen -Wall -Wextra -O3 -DBASE=$(BASE) testtablegen.c && ./build/testtablegen

