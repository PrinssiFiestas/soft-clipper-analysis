BASE ?= 16

CC = gcc
CFLAGS = -Wall -Wextra -ggdb3 -gdwarf -DBASE=$(BASE) -march=native

help:
	@echo 'Targets'
	@echo '  sequence [BASE=<base]       Print full sequence of functions of a given BASE.'
	@echo '  test_sequence BASE=<base>   Count and test all possible sequences of a given base.'
	@echo '  sines [BASE=<base>]         Generate a table of sines of multiples of frequencies.'
	@echo '  plot [BASE=<base>] N=<idx>  Generate CSV of a function of given BASE and index N.'

sequence:
	@mkdir -p build
	@$(CC) -o build/sequence $(CFLAGS) -O3 src/funcs.c && ./build/sequence

test_sequence:
	@mkdir -p build
	@$(CC) -o build/testtablegen $(CFLAGS) -O3 src/testtablegen.c && ./build/testtablegen

sines:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm -O3 src/synthesis.c && ./build/synthesis

plot:
	@mkdir -p build
	@$(CC) -o build/plot $(CFLAGS) -DN=$N -O3 src/plot.c && ./build/plot
