CC = gcc
CFLAGS = -Wall -Wextra -ggdb3 -gdwarf -DBASE=$(BASE) -march=native -fno-math-errno
# Remember to not add -O3 by default, we may want to plot in Seergdb.

ifneq ($(COUNT),)
CFLAGS += "-DCOUNT=$(COUNT)"
endif

ifneq ($(THD),)
CFLAGS += "-DTHD=$(THD)
endif

ifneq ($(CUSTOM),)
CFLAGS += "-DCUSTOM=$(CUSTOM)"
endif

help:
	@echo 'Targets'
	@echo '  sequence BASE=<base> [COUNT=<1|0>]   Print full sequence of functions of a given BASE.'
	@echo '  test_sequence BASE=<base>            Count and test all possible sequences of a given base.'
	@echo '  sines BASE=<base>                    Generate a table of sines of multiples of frequencies.'
	@echo '  plot BASE=<base> N=<idx>             Generate CSV of a function of given BASE and index N.'
	@echo '  plot BASE=<base> CUSTOM=<function>   Generate CSV of a custom C expression e.g. x*x.'
	@echo '  test_thd BASE=<base>                 Test THD calculation.'
	@echo '  test_cdf BASE=<base>                 Test CDF calculation.'
	@echo '  find BASE=<base> [CUSTOM=<function>] Find differences of functions from generated ones.'
	@echo '  clean                                Delete build artifacts.'
	@exit 1

sequence:
	@mkdir -p build
	@$(CC) -o build/sequence $(CFLAGS) -O3 src/sequence.c && ./build/sequence

test_sequence:
	@mkdir -p build
	@$(CC) -o build/testtablegen $(CFLAGS) -O3 src/testtablegen.c && ./build/testtablegen

sines:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis

plot:
	@mkdir -p build
	@$(CC) -o build/plot $(CFLAGS) $(DCUSTOM) -DN=$N src/plot.c && ./build/plot

test_thd:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis > build/sines.c
	@$(CC) -o build/testthd $(CFLAGS) -DTHD_MAIN -O3 -lm src/thd.c && ./build/testthd

test_cdf:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis > build/sines.c
	@$(CC) -o build/testcdf $(CFLAGS) -DCDF_MAIN -lm src/thd.c src/cdf.c && ./build/testcdf

test_rms:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis > build/sines.c
	@$(CC) -o build/testrms $(CFLAGS) -lm src/thd.c src/cdf.c src/rms.c && ./build/testrms

find:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis > build/sines.c
	@$(CC) -o build/finder $(CFLAGS) -lm src/thd.c src/cdf.c src/finder.c src/interpolation.c && ./build/finder

clean:
	rm -rf build
