#include "shared.h"

#ifndef CUSTOM
#  ifndef N
// Index of test function. For BASE=16, 145 is ok. For, BASE=48, 432123 is ok.
#    define N 145
#  endif
#endif

// Define this to plot the second derivative instead of the function.
//#define SECOND_DERIVATIVE

int main(void)
{
    int f[1 + BASE];
    f_init(f);

    #ifndef CUSTOM
    size_t count = 0;
    for (uint32_t state = 1; count < N; ++count)
        if ( ! f_next(&state, f))
            exit(!!fprintf(stderr, "N %i out of bounds. Max N: %zu\n", N, count));
    #else
    for (int i = 0; i < BASE; ++i) {
        double x = (i+.5)/BASE;
        f[i] = BASE*(CUSTOM);
    }
    #endif

    float f_filtered_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* f_filtered = f_filtered_mem + IIR_TAIL_LENGTH + BASE;
    f_filter(f_filtered, f);

    #ifndef SECOND_DERIVATIVE
    for (size_t i = 0; i < BASE; ++i)
        printf("%zu, %f\n", i, f_filtered[i]);
    #endif

    for (int i = 0; i < BASE-1; ++i) // first derivative
        f_filtered[i] = f_filtered[i + 1] - f_filtered[i];
    f_filtered[BASE-1] = f_filtered[BASE-2];
    for (int i = 0; i < BASE-1; ++i) // second derivative
        f_filtered[i] = f_filtered[i + 1] - f_filtered[i];
    f_filtered[BASE-1] = f_filtered[BASE-2];

    #ifdef SECOND_DERIVATIVE
    for (size_t i = 0; i < BASE; ++i)
        printf("%zu, %f\n", i, f_filtered[i]);
    #endif
}
