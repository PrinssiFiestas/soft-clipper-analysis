CC = gcc
CFLAGS = -Wall -Wextra -ggdb3 -gdwarf -Iinclude -DBASE=$(BASE) -march=native -fno-math-errno
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
	@echo '  smoothest BASE<base>                 Find the smoothest clipper with precision BASE.'
	@echo '  sequence BASE=<base> [COUNT=<1|0>]   Print full sequence of functions of a given BASE.'
	@echo '  test_sequence BASE=<base>            Count and test all possible sequences of a given base.'
	@echo '  sines BASE=<base>                    Generate a table of sines of multiples of frequencies.'
	@echo '  plot BASE=<base> N=<idx>             Generate CSV of a function of given BASE and index N.'
	@echo '  plot BASE=<base> CUSTOM=<function>   Generate CSV of a custom C expression e.g. x*x.'
	@echo '  test_thd BASE=<base>                 Test THD calculation.'
	@echo '  test_cdf BASE=<base>                 Test CDF calculation.'
	@echo '  find BASE=<base> [CUSTOM=<function>] Find differences of functions from generated ones.'
	@echo '  shader BASE=<base>                   Check shader compilation errors and stringify shader.'
	@echo '  test_gpu                             Test if GPU implementation matches CPU implementation.'
	@echo '  analyze                              Analyze results.'
	@echo '  clean                                Delete build artifacts.'
	@exit 1

NPROC = $(shell echo `nproc`)
ifneq ($(NPROC),)
CFLAGS += -DNPROC=$(NPROC)
endif

CACHE_LINE_SIZE = $(shell echo `getconf LEVEL1_DCACHE_LINESIZE`)
ifneq ($(GETCONF),)
CFLAGS += -NCACHE_LINE_SIZE=$(CACHE_LINE_SIZE)
endif

SMOOTHEST_SRCS = src/thd.c src/cdf.c src/smoothest.c src/sequence.c src/gpu.c src/glad.c
SMOOTHEST_LFLAGS = -lm -pthread -lX11 -lGLX -lGL

smoothest:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis > build/sines.c
	@$(CC) -o build/shader $(CFLAGS) src/shader.c src/glad.c -lX11 -lGLX -lGL && ./build/shader > ./build/shader_source.c
	@$(CC) -o build/smoothest $(CFLAGS) -O3 $(SMOOTHEST_SRCS) $(SMOOTHEST_LFLAGS) && ./build/smoothest

sequence:
	@mkdir -p build
	@$(CC) -o build/sequence $(CFLAGS) -DSEQUENCE_MAIN -O3 src/sequence.c && ./build/sequence

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
	@$(CC) -o build/finder $(CFLAGS) -lm src/thd.c src/cdf.c src/finder.c && ./build/finder

shader:
	@mkdir -p build
	@$(CC) -o build/shader $(CFLAGS) -O3 src/shader.c src/glad.c -lX11 -lGLX -lGL && ./build/shader

ifneq ($(BENCH),)
CFLAGS += "-DBENCH=$(BENCH)"
else
test_gpu: CFLAGS += -DGPU_WORK_SIZE=1
endif

test_gpu:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis > build/sines.c
	@$(CC) -o build/shader $(CFLAGS) src/shader.c src/glad.c -lX11 -lGLX -lGL && ./build/shader > ./build/shader_source.c
	@$(CC) -o build/testgpu $(CFLAGS) -O3 -DGPU_MAIN src/gpu.c src/glad.c src/thd.c src/cdf.c -lm -lX11 -lGLX -lGL && ./build/testgpu

analyze:
	@mkdir -p build
	@$(CC) -o build/synthesis $(CFLAGS) -lm src/synthesis.c && ./build/synthesis > build/sines.c
	@$(CC) -o build/analyze $(CFLAGS) -lm src/thd.c src/cdf.c src/analysis.c && ./build/analyze

clean:
	rm -rf build
