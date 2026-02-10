#ifndef SHARED_H_INCLUDED
#define SHARED_H_INCLUDED 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

// TODO names are starting to get confusing, fix this!

#ifndef BASE // defined in Makefile, this is here just for clangd.
// Precision of clipping function and unipolar function length. Buffers of this
// size are assumed to have a couple of extra elements at index -1 and -2.
#define BASE 48
#endif

// Number of samples. Should be at least 2*BASE to fit a full sinusoid.
// Bigger makes DFT much more accurate, but clearly needs more processing time.
// Does not need to be a power of two, we use DFT instead of FFT.
#define T (7*BASE)

// Fixed point fractional bits. 24 is guaranteed to overflow, so keep it sane.
#define FIXED_WIDTH 20

// Amplitude of sines or fixed width precision.
#define A (1<<FIXED_WIDTH)

static inline void f_set(int f[1 + BASE], int n)
{
    f[0] = 0;
    for (size_t i = 1; i < 1 + BASE; ++i)
        f[i] = n;
}

static inline void f_print(const int f[1 + BASE])
{
    printf("[%i", f[0]);
    for (size_t i = 1; i < 1 + BASE; ++i)
        printf(" %i", f[i]);
    puts("]");
}

// Next function from function sequence. f_state should be initialized to one.
// f should be initialized using f_set().
static inline bool f_next(size_t* f_state, int f[1 + BASE])
{
    size_t* i = f_state;
    if (f[1] >= BASE)
        return false;

    int d1, d2;
    if (f[*i] < BASE) { // flush from next
        ++*i;
    }
    else do { // flush from left
        --*i;
        d1 = f[*i-0] - f[*i-1];
        d2 = f[*i-1] - f[*i-2];
    } while (d1 == d2);

    int inc = f[*i] + 1;
    for (size_t j = *i; j < 1 + BASE; ++j) // flush
        f[j] = inc;
    return true;
}

// Filtering makes the edges of the function go crazy, this is length of
// extrapolation at the edges.
#define IIR_TAIL_LENGTH (IIR_POLES << 2)

// Both of these increase smoothing. IIR_INTENSITY also lowers precision, so
// shouldn't be too high. Higher IIR_POLES is more expensive, also should be
// modest. More smoothing, better quality second derivative, but too much
// smoothing make all functions the same.
#define IIR_INTENSITY 2
#define IIR_POLES 4

// Scale to fixed width and smooth out crap precision with IIR filter.
static inline void f_preprocess(int f_out[restrict], const int f_in[restrict BASE])
{
    int f_right_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    int* f_right = f_right_mem + IIR_TAIL_LENGTH + BASE;

    for (size_t i = 0; i <= BASE; ++i) // copy and scale positive side
        f_out[i] = f_right[i] = f_in[i] << FIXED_WIDTH;
    for (size_t i = BASE; i < 1 + BASE + IIR_TAIL_LENGTH; ++i) // extrapolate
        f_out[i] = f_right[i] = f_out[BASE];
    for (size_t i = 1; i < 1 + BASE + IIR_TAIL_LENGTH; ++i) // mirror negative side
        f_out[-i] = f_right[-i] = -f_out[i];

    for (size_t k = 0; k < IIR_POLES; ++k) {
        // IIR filtering from right and left
        for (int i = 1 + BASE + IIR_TAIL_LENGTH - 2; i >= -BASE - IIR_TAIL_LENGTH; --i)
            f_right[i] = (f_right[i]>>IIR_INTENSITY) + (f_right[i + 1]>>1);
        for (int i = -1 - BASE - IIR_TAIL_LENGTH + 1; i <= BASE + IIR_TAIL_LENGTH; ++i)
            f_out[i] = (f_out[i]>>IIR_INTENSITY) + (f_out[i - 1]>>1);

        // Combine for zero phase
        for (int i = -BASE - IIR_TAIL_LENGTH; i <= BASE + IIR_TAIL_LENGTH; ++i)
            f_out[i] += f_right[i];
    }
}

// TODO float implementation of f_preprocess().

#endif // SHARED_H_INCLUDED
