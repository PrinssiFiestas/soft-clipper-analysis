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

    int f_filtered_mem[BASE + 2];
    int* f_filtered = f_filtered_mem + 2;
    f_preprocess(f_filtered, f);
    for (size_t i = 0; i < BASE; ++i)
        printf("%zu, %i\n", i, f_filtered[i]);
}
