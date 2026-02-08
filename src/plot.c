#include "shared.h"

// Index of test function. For BASE=16, 145 is ok. For, BASE=48, 432123 is ok.
#ifndef N
#define N 432
#endif

int main(void)
{
    int f_mem[BASE + 2] = {0};
    int* f = f_mem + 2;
    set_f(f, 1);

    for (size_t state = 0, i = 0; i < N; ++i)
        if ( ! next_f(&state, f))
            exit(!!fprintf(stderr, "N %i out of bounds.\n", N));

    // Scale
    for (size_t i = 0; i < BASE; ++i)
        f[i] = (i+1) << 12; // f[i] <<= 12;

    // IIR from right
    int f_iir_mem[BASE + 2];
    int* f_iir = f_iir_mem + 2;
    f_iir[BASE - 1] = f[BASE - 1];
    for (size_t i = BASE-1 - 1; i+3 > 0; --i)
        f_iir[i] = (f[i]>>1) + (f_iir[i+1]>>1);

    // IIR from left
    f[-1] = -f[0];
    for (size_t i = 0; i != BASE; ++i)
        f[i] = (f[i]>>1) + (f[i-1]>>1);

    // IIR combine
    for (size_t i = -1; i < BASE; ++i)
        f[i] = (f[i] + f_iir[i]) >> 1;
    f[-1] = 0;

    for (size_t i = 0; i <= BASE; ++i)
        printf("%zu, %i\n", i, f[i - 1]);
}
