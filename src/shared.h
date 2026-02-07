#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifndef BASE // defined in Makefile, this is here just for clangd.
// Precision of clipping function and unipolar length.
#define BASE 5
#endif

// Bipolar buffer size.
#define T (2*BASE + 1)

// Amplitude of sines or fixed width precision. Since using integer math, bigger
// amplitude would give better precision, however, our clippers will be horribly
// imprecise anyway, so no need to go crazy. Smaller value prevents overflow.
#define A (1<<12)

static inline void set_f(int f[BASE], int n)
{
    for (size_t i = 0; i < BASE; ++i)
        f[i] = n;
}

static inline void put_f(const int f[BASE])
{
    printf("[%i", f[0]);
    for (size_t i = 1; i < BASE; ++i)
        printf(" %i", f[i]);
    puts("]");
}

static inline bool valid_f(const int f[BASE])
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

static inline bool next_f(size_t* i, int f[BASE])
{
    if (f[0] >= BASE)
        return false;

    int d1, d2;
    if (f[*i] < BASE) { // flush right
        ++*i;
    }
    else do { // flush left
        --*i;
        d1 = f[*i-0] - f[*i-1];
        d2 = f[*i-1] - f[*i-2];
    } while (d1 == d2);

    int inc = f[*i] + 1;
    for (size_t j = *i; j < BASE; ++j) // flush
        f[j] = inc;
    return true;
}
