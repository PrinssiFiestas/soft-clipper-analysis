#include "shared.h"
#include "../build/sines.c"
#include <math.h>

// TODO we only need odd harmonics.

// Calculates squared THD from harmonics' amplitudes.
float coeffs_thd(size_t coeffs_length, const int coeffs[T/2])
{
    int64_t sum = 0;
    for (size_t i = 2; i < coeffs_length; ++i)
        sum += (int64_t)coeffs[i]*coeffs[i];

    return (float)sum / ((int64_t)coeffs[1]*coeffs[1]);
}

// Calculates squared THD of a given signal.
float x_thd(const int x[T])
{
    int bs[T/2];
    bs[0] = 0;
    size_t k = 1;

    const size_t SKIP = 2; // last harmonics are likely to not contribute much.
    for (; k < T/2 - SKIP; ++k) { // TODO detect change for early return?
        int64_t b = 0;
        for (size_t t = 0; t < T; ++t)
            b += (int64_t)x[t] * sines[k][t];
        bs[k] = b >> FIXED_WIDTH;
    }
    return coeffs_thd(k, bs);
}

// Calculates squared THD of clipper using DFT
float f_thd(const int f[1 + BASE], float in_gain)
{
    int x[T];
    for (size_t t = 0; t < T; ++t) {
        float sint = BASE*in_gain*sine[t];
        float floor = floorf(sint);
        float fract = sint - floor;
        int i = floor;
        if (i >= BASE)
            x[t] = f[BASE];
        else if (i < -BASE)
            x[t] = f[-BASE];
        else
            x[t] = (1.f-fract)*f[i] + fract*f[i + 1];
    }
    return x_thd(x);
}

// THD of the Blunter calculated from a closed form DFT coefficient formula.
float blunter_thd(size_t n_harmonics)
{
    int bs[T/2] = {0};
    for (size_t k = 1; k < n_harmonics; k += 2) { // only need odds
        double b = 8. / (M_PI*k*k*k - 4.*M_PI*k);
        bs[k] = b * (1 << FIXED_WIDTH);
    }
    bs[1] += 2 << FIXED_WIDTH;
    return coeffs_thd(n_harmonics, bs);
}

float normalized_input_gain(const int f[1 + BASE], float thd)
{
    // Most counter generated functions clip somewhere after BASE/2.
    float input_gain = (float)BASE/2;
    // TODO implementation
    return input_gain; (void)f; (void)thd;
}

int main(void)
{
    #define TESTS
    #ifdef TESTS
    // Test basic THD calculation from signal.
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
            fprintf(stderr, "Test signal THD calculation [FAILED]\n");
            fprintf(stderr, "Expected %g\n", thd_from_coeffs);
            fprintf(stderr, "Got      %g\n", thd_from_signal);
            fprintf(stderr, "Increase T if this failed due to imprecision.\n");
            exit(EXIT_FAILURE);
        }
    }

    int blunter_mem[BASE + 1 + BASE];
    int* blunter = blunter_mem + BASE;
    for (int i = -BASE; i <= BASE; ++i) {
        double x = (i+.5) / BASE;
        blunter[i] = (2.*x - fabs(x)*x) * (1 << FIXED_WIDTH);
    }

    // Test clipper function THD calculation.
    {
        float thd_closed_form = blunter_thd(T/2 - 2);
        float thd_measured    = f_thd(blunter, 1.f);
        if (fabsf(thd_closed_form - thd_measured) > .001f) {
            fprintf(stderr, "Blunter THD calculation [FAILED]\n");
            fprintf(stderr, "Expected %g\n", thd_closed_form);
            fprintf(stderr, "Got      %g\n", thd_measured);
            exit(EXIT_FAILURE);
        }
    }

    // Test and bench input gain normalization
    {
    }
    puts("THD tests [PASSED]");
    #endif // TESTS
}
