CC = gcc
CFLAGS = -Wall -Wextra -ggdb3 -gdwarf -DBASE=$(BASE) -march=native
# Remember to not add -O3 by default, we may want to plot in Seergdb.

help:
	@echo 'Targets'
	@echo '  sequence BASE=<base>                Print full sequence of functions of a given BASE.'
	@echo '  test_sequence BASE=<base>           Count and test all possible sequences of a given base.'
	@echo '  sines BASE=<base>                   Generate a table of sines of multiples of frequencies.'
	@echo '  plot BASE=<base> N=<idx>            Generate CSV of a function of given BASE and index N.'
	@echo '  plot BASE=<base> CUSTOM=<function>  Generate CSV of a custom C expression e.g. x*x.'
	@echo '  clean                               Delete build artifacts.'
	@exit 1

sequence:
	@mkdir -p build
	@$(CC) -o build/sequence $(CFLAGS) -O3 src/funcs.c && ./build/sequence

test_sequence:
	@mkdir -p build
	@$(CC) -o build/testtablegen $(CFLAGS) -O3 src/testtablegen.c && ./build/testtablegen

sines:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis

ifneq ($(CUSTOM),)
DCUSTOM = "-DCUSTOM=$(CUSTOM)"
endif
plot:
	@mkdir -p build
	@$(CC) -o build/plot $(CFLAGS) $(DCUSTOM) -DN=$N src/plot.c && ./build/plot

clean:
	rm -rf build
