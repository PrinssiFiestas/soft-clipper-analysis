#include "shared.h"
#include "../build/sines.c"

#define TESTS // for THD unit tests
// #define THD_PLOT // generate CSV describing THD as a function of input gain
#define BENCH // benchmark
// #define PLOT_IN_GAINS // to see that most in gains do fall well below 1.

#define SKIP 2 // harmonics close to Nyquist are likely to be dominated by noise.

#ifdef BENCH
size_t g_dft_coeff_calculation_count = 0;
size_t g_max_secant_iterations = 0;
size_t g_total_secant_iterations = 0;
#endif

// Calculates squared THD from harmonics' amplitudes.
float coeffs_thd(size_t coeffs_length, const fixed_t coeffs[T/4])
{
    int64_t sum = 0;
    for (size_t i = 1; i < coeffs_length; ++i)
        sum += (int64_t)coeffs[i]*coeffs[i];

    if (sum == 0) // very unlikey, but possible for clippers with large linear
        sum = 1;  // region. Zero would cause integer zero division later.

    return (float)sum / ((int64_t)coeffs[0]*coeffs[0]);
}

// Calculates squared THD of a given signal.
float x_thd(const fixed_t x[T])
{
    fixed_t bs[T/4];
    size_t k = 0;

    for (; k < T/4 - SKIP; ++k) {
        #ifdef BENCH
        ++g_dft_coeff_calculation_count;
        #endif
        int64_t b = 0;
        for (size_t t = 0; t < T; ++t)
            b += (int64_t)x[t] * sines[k][t];
        bs[k] = b >> FIXED_WIDTH;

        // k must be large enough to ensure that it is safe to ignore the rest
        // without too much loss of accuracy.
        if (k > 2 && is_equal_fixed(bs[k], bs[k - 1], .1*A))
            break;
    }
    return coeffs_thd(k, bs);
}

// Calculates squared THD of clipper using DFT
float f_thd(const float f[restrict], float in_gain)
{
    fixed_t x[T];
    for (size_t t = 0; t < T; ++t)
        x[t] = A*f_call(f, in_gain*sine[t]);

    return x_thd(x);
}

// THD of the Blunter calculated from a closed form DFT coefficient formula.
float blunter_thd(size_t n_harmonics)
{
    fixed_t bs[T/4] = {0};
    for (size_t i = 0; i < n_harmonics; ++i) {
        size_t k = 2*i + 1;
        double b = 8. / (M_PI*k*k*k - 4.*M_PI*k);
        bs[i] = b * (1 << FIXED_WIDTH);
    }
    bs[0] += 2 << FIXED_WIDTH;
    return coeffs_thd(n_harmonics, bs);
}

// Returns an input gain such that f_thd(f, input_gain) ≈ THD_NORMALIZED.
float normalized_input_gain(const float f[1 + BASE])
{
    // Plotting many clipper's THD's as functions of input gains showed that
    // most clippers have close to zero THD when x < 0.3f. Same plots revealed
    // that the average of x == .6f got same THD as the Blunter's THD. So we
    // start our estimate with that average and use the minimum to get next data
    // point.
    const float x_min = .3f;
    const float y_min = -THD_NORMALIZED; // can't have THD < 0
    const float x_avg = .6f;

    float x0 = x_avg;
    float y0 = f_thd(f, x0) - THD_NORMALIZED;

    // Line between average minimum point (x_min, y_min) to first data point
    // (x0, y0) is described by k*(x-x_min) + y0.
    float k = (y0 - y_min) / (x0 - x_min);

    // Solving for next x1 from k*(x1-x_min) + y0 == 0 gives us
    float x1 = x_min - y_min/k;
    float y1 = f_thd(f, x1) - THD_NORMALIZED;

    // We have two data points now, use secant method to find final result fast.
    // First secant iterations might throw x to negative root. This is fine
    // because f_thd(f, -kx) == f_thd(f, kx), but the results will be confusing
    // down the line, so return absolute value.
    float x2 = x1;
    float y2 = y1;
    size_t secant_iterations = 0;
    while (fabsf(y2) > .01f * THD_NORMALIZED) {
        secant_iterations++;
        #ifdef BENCH
        if (secant_iterations > g_max_secant_iterations)
            g_max_secant_iterations = secant_iterations;
        g_total_secant_iterations++;
        #endif
        if (secant_iterations > 10 || y1 == y0) {
            if (y2 > 0.f) // returning unfair normalization is ok.
                return fabsf(x2);
            else // return high value that discards f.
                return 1e10;
        }
        x2 = x1 - y1 * (x1 - x0) / (y1 - y0);
        y2 = f_thd(f, x2) - THD_NORMALIZED;
        x0 = x1;
        x1 = x2;
        y0 = y1;
        y1 = y2;
    }
    return fabsf(x2);
}

#ifdef THD_MAIN
int main(void)
{
    #if PLOT_IN_GAINS
    int f_gen[1 + BASE];
    f_init(f_gen);
    float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* f = f_mem + IIR_TAIL_LENGTH + BASE;
    uint64_t i = 0;
    for (uint32_t f_gen_state = 1; f_next(&f_gen_state, f_gen); ++i) {
        f_filter(f, f_gen);
        float in_gain = normalized_input_gain(f);
        if (in_gain > 2.f)
            in_gain = 2.f;
        printf("%zu, %g\n", i, in_gain);
    }
    exit(0);
    #endif // PLOT_IN_GAINS

    #ifdef BENCH
    {
        int f_gen[1 + BASE];
        f_init(f_gen);
        float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
        float* f = f_mem + IIR_TAIL_LENGTH + BASE;

        __uint128_t t_start = time_begin();
        __uint128_t t_filter_total = 0;
        __uint128_t t_thd_total    = 0;
        float min_in_gain = INFINITY;
        float max_in_gain = 0.f;

        uint64_t count = 0;
        for (uint32_t f_gen_state = 1; f_next(&f_gen_state, f_gen); ++count)
        {
            __uint128_t t_filter = time_begin();
            __asm__ __volatile__("":::"memory");
            f_filter(f, f_gen);
            __asm__ __volatile__("":::"memory");
            t_filter_total += time_begin() - t_filter;
            __asm__ __volatile__("":::"memory");
            __uint128_t t_thd = time_begin();
            __asm__ __volatile__("":::"memory");
            float in_gain = normalized_input_gain(f);
            __asm__ __volatile__("":::"memory");
            t_thd_total += time_begin() - t_thd;
            min_in_gain = fminf(min_in_gain, in_gain);
            max_in_gain = fmaxf(max_in_gain, in_gain);
        }
        double time = time_diff(t_start);

        printf("Done benchmarking.\n");
        printf("Min input gain: %g\n", min_in_gain);
        printf("Max input gain: %g\n", max_in_gain);
        printf("Total time:     %g\n", time);
        printf("Filtering time: %g\n", (double)t_filter_total / 1000000000.);
        printf("THD time:       %g\n", (double)t_thd_total / 1000000000.);
        printf("DFT coefficients calculated %zu times.\n", g_dft_coeff_calculation_count);
        printf("Max secant iterations:     %zu\n", g_max_secant_iterations);
        printf("Average secant iterations: %g\n", (double)g_total_secant_iterations / count);
        printf("Total secant iterations:   %zu\n", g_total_secant_iterations);
    }
    #endif

    #ifdef THD_PLOT
    {
        int f_gen[1 + BASE];
        f_init(f_gen);

        float func_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
        float* f = func_mem + IIR_TAIL_LENGTH + BASE;
        size_t skip = 0;

        for (float input_gain = .0125f; input_gain <= 1.f; input_gain += .0125f) {
            f_init(f_gen);
            for (uint32_t f_gen_state = 1; f_next(&f_gen_state, f_gen); ) {
                f_filter(f, f_gen);
                if ((skip++ & ((1<<5)-1)) == 0)
                    printf("%g, %g\n", input_gain, f_thd(f, input_gain));
            }
        }
    }
    #endif // THD_PLOT

    #ifdef TESTS
    assert(is_equal_float(30.05f, 30.f, .01f));
    assert( ! is_equal_float(30.5f, 30.f, .01f));
    assert(is_equal_fixed(30.05f * A, 30.f * A, .01f * A));
    assert( ! is_equal_fixed(30.5f * A, 30.f * A, .01f * A));

    // Test basic THD calculation from signal.
    {
        fixed_t test_signal[T] = {0};
        fixed_t test_coeffs[T/4] = {714, 434, 887, 133, -95, 556, 34, -405};
        size_t coeffs_length = 8;
        for (size_t k = 0; k < coeffs_length; ++k)
            for (size_t t = 0; t < T; ++t)
                test_signal[t] += ((int64_t)test_coeffs[k] * sines[k][t]) >> FIXED_WIDTH ;

        float thd_from_coeffs = coeffs_thd(coeffs_length, test_coeffs);
        float thd_from_signal = x_thd(test_signal);
        if ( ! is_equal_float(thd_from_coeffs, thd_from_signal, .01f)) {
            fprintf(stderr, "Test signal THD calculation [FAILED]\n");
            fprintf(stderr, "Expected %g\n", thd_from_coeffs);
            fprintf(stderr, "Got      %g\n", thd_from_signal);
            fprintf(stderr, "Increase T if this failed due to imprecision.\n");
            exit(EXIT_FAILURE);
        }
    }

    float blunter_mem[BASE + 1 + BASE];
    float* blunter = blunter_mem + BASE;
    for (int i = -BASE; i <= BASE; ++i) {
        double x = (i+.5) / BASE;
        blunter[i] = 2.*x - fabs(x)*x;
    }
    float thd_closed_form = blunter_thd(T/4 - SKIP);

    // Test clipper function THD calculation.
    {
        float thd_measured = f_thd(blunter, 1.f);
        if ( ! is_equal_float(thd_closed_form, thd_measured, .01f)) {
            fprintf(stderr, "Blunter THD calculation [FAILED]\n");
            fprintf(stderr, "Expected %g\n", thd_closed_form);
            fprintf(stderr, "Got      %g\n", thd_measured);
            exit(EXIT_FAILURE);
        }
    }

    // Test input gain normalization
    {
        float blunter2_mem[BASE + 1 + BASE];
        float* blunter2 = blunter2_mem + BASE;
        double embedded_input_gain = 1.7; // to be normalized away
        for (int i = -BASE; i <= BASE; ++i) {
            double x = embedded_input_gain * (i+.5) / BASE;
            if (fabs(x) < 1.)
                blunter2[i] = 2.*x - fabs(x)*x;
            else if (x < 0)
                blunter2[i] = -1.f;
            else
                blunter2[i] = 1.f;
        }

        float thd_measured = f_thd(blunter2, 1.f);
        assert(thd_measured > (embedded_input_gain - .1f) * thd_closed_form);
        float input_gain = normalized_input_gain(blunter2);
        thd_measured = f_thd(blunter2, input_gain);
        if ( ! is_equal_float(thd_closed_form, thd_measured, .01f)) {
            fprintf(stderr, "Input gain normalization [FAILED]\n");
            fprintf(stderr, "Expected in gain %g\n", 1.f/embedded_input_gain);
            fprintf(stderr, "Got in gain      %g\n", input_gain);
            fprintf(stderr, "Expected THD     %g\n", thd_closed_form);
            fprintf(stderr, "Got THD          %g\n", thd_measured);
            exit(EXIT_FAILURE);
        }
    }

    puts("THD tests [PASSED]");
    printf("Blunter THD: %g\n", blunter_thd(T/4)); // 0.0222559f
    #endif // TESTS
}
#endif // THD_MAIN
