#include "shared.h"
#include "../build/sines.c"
#include <math.h>

// Calculates squared THD from harmonics' amplitudes.
float coeffs_thd(size_t coeffs_length, const int coeffs[T/2])
{
    int64_t sum = 0;
    for (size_t i = 2; i < coeffs_length; ++i)
        sum += (int64_t)coeffs[i]*coeffs[i];

    return (float)(sum >> FIXED_WIDTH)
        / ((int64_t)(coeffs[1]*coeffs[1]) >> FIXED_WIDTH);
}

// Calculates squared THD of a given signal.
float x_thd(const int x[T])
{
    int bs[T/2] = {0};
    size_t k = 1;

    const size_t SKIP = 2; // last harmonics are likely to not contribute much.
    for (; k < T/2 - SKIP; ++k) { // TODO detect change for early return?
        int64_t b = 0;
        for (size_t t = 0; t < T; ++t)
            b += ((int64_t)x[t] * sines[k][t]) >> FIXED_WIDTH;
        bs[k] = b >> FIXED_WIDTH;
    }
    return coeffs_thd(k, bs);
}

// // Calculates squared THD of clipper using DFT.
// float f_thd(const int f[1 + BASE])
// {
//     return 0.f;
// }

int main(void)
{
    #define TESTS
    #ifdef TESTS
    {
        int test_signal[T] = {0};
        int test_coeffs[T/2] = {0, 434, 887, 133, -95, 556, 34, -405};
        size_t coeffs_length = 8;
        for (size_t k = 1; k < coeffs_length; ++k)
            for (size_t t = 0; t < T; ++t)
                test_signal[t] += ((int64_t)test_coeffs[k] * sines[k][t]) >> FIXED_WIDTH ;

        float thd_from_coeffs = coeffs_thd(coeffs_length, test_coeffs);
        float thd_from_signal = x_thd(test_signal);
        if (fabsf(thd_from_coeffs - thd_from_signal) > .001f) {
            fprintf(stderr, "THD calculation [FAILED]\n");
            fprintf(stderr, "Expected %g\n", thd_from_coeffs);
            fprintf(stderr, "Got      %g\n", thd_from_signal);
            exit(EXIT_FAILURE);
        }

        puts("THD tests [PASSED]");
    }
    #endif // TESTS
}
