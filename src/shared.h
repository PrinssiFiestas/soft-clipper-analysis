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

// Bipolar buffer size.
#define T (2*BASE + 1)

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

// Next function from function sequence.
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

// Scale to fixed width and smooth out crap precision with IIR filter. f_in[0]
// will be lost due to biasing.
static inline void f_preprocess(int f_out[restrict BASE], const int f_in[restrict BASE])
{
    int f_iir_right[BASE + 2];
    int* f_right = f_iir_right + 2;

    // Scale
    for (size_t i = 0; i < BASE; ++i)
        f_out[i] = f_right[i] = f_in[i] << FIXED_WIDTH;

    // IIR right
    for (size_t i = BASE-1-1; i + 1 > 0; --i)
        f_right[i] = (f_right[i]>>1) + (f_right[i+1]>>1);

    // IIR left
    f_out[-1] = -(f_in[0] << FIXED_WIDTH); // bias to remove kink at i=0.
    for (size_t i = 0; i < BASE; ++i)
        f_out[i] = (f_out[i]<<1) + (f_out[i-1]>>1);

    // Combine left and right
    for (size_t i = 0; i < BASE; ++i)
        f_out[i] = (f_out[i] + f_right[i]) >> 1;
}

#endif // SHARED_H_INCLUDED
