#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifndef BASE
#define BASE 5
#endif

static inline void set_f(int f[BASE], int n)
{
    for (size_t i = 0; i < BASE; ++i)
        f[i] = n;
}

static inline void put_f(const int f[BASE])
{
    char str[BASE + sizeof""];

    for (size_t i = 0; i < BASE; ++i) {
        str[i] = f[i] + '0';
    }
    str[BASE] = '\0';

    puts(str);
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

static inline void inc_flush(int f[], size_t i)
{
    int inc = f[i] + 1;
    for (; i < BASE; ++i)
        f[i] = inc;
}

static inline bool next_f(size_t* i, int f[BASE])
{
    if (f[0] >= BASE)
        return false;

    if (f[*i] < BASE && *i < BASE) { // `i < BASE` is unnecessary?
        inc_flush(f, ++*i);
        return true;
    }
    // else increment based on derivative
    int d1, d2;
    do {
        --*i;
        d1 = f[*i-0] - f[*i-1];
        d2 = f[*i-1] - f[*i-2];
    } while (d1 == d2);

    inc_flush(f, *i);
    return true;
}
