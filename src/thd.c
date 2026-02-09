#include "shared.h"
#include "../build/sines.c"

// Calculates squared THD from harmonics' amplitudes.
float coeffs_thd(size_t coeffs_length, const int coeffs[])
{
    int sum = 0;
    for (size_t i = 1; i < coeffs_length; ++i)
        sum += coeffs[i]*coeffs[i];

    return (float)sum / (coeffs[0]*coeffs[0]);
}

// Calculates squared THD of a given signal.
float x_thd(const int x[T])
{
    int bs[T/2];
    bs[0] = 0;
    size_t k = 1;

    for (; k < T/2 - 1; ++k) // TODO early return?
        for (size_t i = 0; i < T; ++i)
            bs[i] += (x[i] * sines[k][i]) >> FIXED_WIDTH; // TODO do we need the shift?

    return coeffs_thd(k, bs);
}

// // Calculates squared THD of clipper using DFT.
// float f_thd(const int f[restrict BASE])
// {
//
//     return coeffs_thd(b_length, bs;
// }

int main(void)
{
}

