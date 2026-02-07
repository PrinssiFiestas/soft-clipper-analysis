all:
	gcc -o funcs -ggdb3 -gdwarf funcs.c

run:
	gcc -o funcs-fast -O3 funcs.c && ./funcs-fast
