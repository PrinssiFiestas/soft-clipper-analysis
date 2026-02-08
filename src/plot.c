#include "shared.h"

// Index of test function. For BASE=16, 145 is ok. For, BASE=48, 432123 is ok.
#ifndef N
#define N 145
#endif

int main(void)
{
    int f_mem[BASE + 2] = {0};
    int* f = f_mem + 2;
    f_set(f, 1);

    size_t count = 0;
    for (size_t state = 0; count < N; ++count)
        if ( ! f_next(&state, f))
            exit(!!fprintf(stderr, "N %i out of bounds. Max N: %zu\n", N, count));

    // Make fixed for IIR filtering
    for (size_t i = 0; i < BASE; ++i)
        f[i] <<= FIXED_WIDTH;

    // IIR from right
    int f_iir_mem[BASE + 2];
    int* f_iir = f_iir_mem + 2;
    f_iir[BASE - 1] = f[BASE - 1];
    for (size_t i = BASE-1 - 1; i+3 > 0; --i)
        f_iir[i] = (f[i]>>1) + (f_iir[i+1]>>1);

    // IIR from left
    f[-1] = -f[0]; // bias to remove kink at i=0. We also lose data at i=0.
    for (size_t i = 0; i != BASE; ++i)
        f[i] = (f[i]>>1) + (f[i-1]>>1);

    // IIR combine
    for (size_t i = -1; i < BASE; ++i)
        f[i] = (f[i] + f_iir[i]) >> 1;
    f[-1] = 0;

    for (size_t i = 0; i <= BASE; ++i)
        printf("%zu, %i\n", i, f[i - 1]);
}
