#ifndef SHARED_H_INCLUDED
#define SHARED_H_INCLUDED 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifndef BASE // defined in Makefile, this is here just for clangd.
// Precision of clipping function and unipolar function length. Buffers of this
// size are assumed to have a couple of extra elements at index -1 and -2.
#define BASE 48
#endif

// Number of samples. Should be at least 2*BASE to fit a full sinusoid.
// Slightly bigger adds accuracy, but not by a lot due to inaccuracy of clipper
// function. Bigger also needs more processing time obviously. Does not need to
// be a power of two, we use DFT instead of FFT.
#define T (3*BASE)

#define SAMPLE_RATE (1/T)

// Fixed point bit width. No need to be crazy precise, our clipper is horrible
// anyway, so let it be smaller to prevent overflow.
#define FIXED_WIDTH 12

// Amplitude of sines or fixed width precision.
#define A (1<<FIXED_WIDTH)

static inline void f_set(int f[BASE], int n)
{
    for (size_t i = 0; i < BASE; ++i)
        f[i] = n;
}

static inline void f_print(const int f[BASE])
{
    printf("[%i", f[0]);
    for (size_t i = 1; i < BASE; ++i)
        printf(" %i", f[i]);
    puts("]");
}

// Checks if function is increasing and it's derivative is decreasing.
static inline bool f_valid(const int f[BASE])
{
    bool value_increasing = true;
    bool diff_decreasing  = true;
    int  diff = f[0];

    for (size_t i = 0; i < BASE; ++i) {
        value_increasing = f[i] >= f[i-1];
        diff_decreasing  = f[i] - f[i-1] <= diff;
        if (!value_increasing || !diff_decreasing)
            return false;
        diff = f[i] - f[i-1];
    }

    return true;
}

// Next function from function sequence. // TODO indexing
static inline bool f_next(size_t* i, int f[BASE])
{
    if (f[0] >= BASE)
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
    for (size_t j = *i; j < BASE; ++j) // flush
        f[j] = inc;
    return true;
}

// Filtering makes the edges of the function go crazy, this is length of
// extrapolation at the edge.
#define IIR_TAIL_LENGTH (IIR_POLES << 2)

// Both of these increase smoothing. IIR_INTENSITY also lowers precision, so
// shouldn't be too high. Higher IIR_POLES is more expensive, also should be
// modest. More smoothing, better quality second derivative, but too much
// smoothing make all functions the same.
#define IIR_INTENSITY 2
#define IIR_POLES 4

// Scale to fixed width and smooth out crap precision with IIR filter. f_in[0]
// will be lost due to biasing.
static inline void f_preprocess(int f_out[restrict], const int f_in[restrict BASE])
{
    int f_iir_right[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    int* f_right = f_iir_right + IIR_TAIL_LENGTH + BASE;

    for (size_t i = 0; i <= BASE; ++i) // copy and scale positive side
        f_out[i] = f_right[i] = f_in[i] << FIXED_WIDTH;
    for (size_t i = BASE; i < 1 + BASE + IIR_TAIL_LENGTH; ++i) // fill tail
        f_out[i] = f_right[i] = f_out[BASE];
    for (size_t i = 1; i < 1 + BASE + IIR_TAIL_LENGTH; ++i) // mirror negative side
        f_out[-i] = f_right[-i] = -f_out[i];

    for (size_t k = 0; k < IIR_POLES; ++k) {
        // IIR filtering from right and left
        for (int i = 1 + BASE + IIR_TAIL_LENGTH - 1; i >= -BASE - IIR_TAIL_LENGTH; --i)
            f_right[i] = (f_right[i]>>IIR_INTENSITY) + (f_right[i + 1]>>1);
        for (int i = -1 - BASE - IIR_TAIL_LENGTH + 1; i <= BASE + IIR_TAIL_LENGTH; ++i)
            f_out[i] = (f_out[i]>>IIR_INTENSITY) + (f_out[i - 1]>>1);

        // Combine for zero phase
        for (int i = -BASE - IIR_TAIL_LENGTH; i <= BASE + IIR_TAIL_LENGTH; ++i)
            f_out[i] += f_right[i];
    }

    #if 0
    int f_iir_right[2 + BASE + 2];
    int* f_right = f_iir_right + 2;

    // Scale
    for (size_t i = 0; i < BASE; ++i)
        f_out[i] = f_right[i] = f_in[i] << FIXED_WIDTH;
    f_out[BASE] = f_right[BASE] = f_out[BASE-1] >> 2;

    // IIR right
    for (size_t i = BASE-1; i + 1 > 0; --i)
        f_right[i] = (f_right[i]>>2) + (f_right[i+1]>>1);

    // IIR left
    f_out[-1] = -(f_in[0] << FIXED_WIDTH); // bias to remove kink at i=0.
    for (size_t i = 0; i < BASE; ++i)
        f_out[i] = (f_out[i]>>2) + (f_out[i-1]>>1);

    // Combine left and right
    for (size_t i = 0; i < BASE; ++i)
        f_out[i] = f_out[i] + f_right[i];

    // The second derivative is very sensitive to noise, filter once more.

    for (size_t i = BASE-1; i + 1 > 0; --i)
        f_right[i] = (f_right[i]>>2) + (f_right[i+1]>>1);
    //f_out[-1] = -(f_in[0] << FIXED_WIDTH);
    for (size_t i = 0; i < BASE; ++i)
        f_out[i] = (f_out[i]>>2) + (f_out[i-1]>>1);
    for (size_t i = 0; i < BASE; ++i)
        f_out[i] = f_out[i] + f_right[i];
    #endif
}

#endif // SHARED_H_INCLUDED
