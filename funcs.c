#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#ifndef BASE
#define BASE 5
#endif

void put_fp(const int f[])
{
    char str[BASE + sizeof""];
    bool value_increasing = true;
    bool diff_decreasing  = true;
    int  diff = f[0];

    for (size_t i = 0; i < BASE; ++i) {
        str[i] = f[i] + '0';
        value_increasing |= f[i] >= f[i-1];
        diff_decreasing  |= f[i] - f[i-1] <= diff;
        diff = f[i] - f[i-1];
    }
    str[BASE] = '\0';

    puts(str);
    assert(value_increasing);
    assert(diff_decreasing);
}

void inc_flush(int f[], size_t i)
{
    int inc = f[i] + 1;
    for (; i < BASE; ++i)
        f[i] = inc;
}

void count(int f[])
{
    f[-2] = f[-1] = 0;
    for (size_t i = 0; i < BASE; ++i)
        f[i] = 1;
    put_fp(f);

    for (size_t i = 0; f[0] < BASE; put_fp(f))
    {
        if (f[i] < BASE && i < BASE) { // `i < BASE` is unnecessary?
            inc_flush(f, ++i);
            continue;
        }
        // else increment based on derivative
        int d1, d2;
        do {
            --i;
            d1 = f[i-0] - f[i-1];
            d2 = f[i-1] - f[i-2];
        } while (d1 == d2);

        inc_flush(f, i);
    }
}

int main(void)
{
    int f[BASE/*negs*/ + 1/*zero*/ + BASE/*poss*/] = {0};
    int*const fp = f + BASE + 1; // positive side of f

    count(fp);
}
