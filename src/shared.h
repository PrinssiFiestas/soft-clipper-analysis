#ifndef SHARED_H_INCLUDED
#define SHARED_H_INCLUDED 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// Fixed point type.
typedef int fixed_t;

#ifndef BASE // defined in Makefile, this is here just for clangd.
// Generator number base. This will also determine the precision of the
// generated clipping function.
#define BASE 48
#endif

// Number of samples. Should be at least 2*BASE to fit a full sinusoid.
// Bigger makes DFT much more accurate, but clearly needs more processing time.
// Does not need to be a power of two, we use DFT instead of FFT.
#define T (7*BASE)

// Fixed point fractional bits. 24 is guaranteed to overflow, so keep it sane.
#define FIXED_WIDTH 20

// Amplitude of sines or fixed width precision.
#define A (1<<FIXED_WIDTH)

// We'll normalize agains Blunter's THD by default.
#define THD_NORMALIZED 0.0222559f

// Initialize clipper function generator.
static inline void f_init(int f[1 + BASE])
{
    //All zeros doesn't make sense, so start with step to one. Starting from a
    // seemingly hard clipper seems counter intuitive, but the IIR filter turns
    // it into a soft clipper. The first ones will also differ from some
    // seemingly equivalent ones with more gain due to the filtering.
    f[0] = 0;
    for (size_t i = 1; i < 1 + BASE; ++i)
        f[i] = 1;
}

static inline void f_print(const int f[1 + BASE])
{
    printf("[%i", f[0]);
    for (size_t i = 1; i < 1 + BASE; ++i)
        printf(" %i", f[i]);
    puts("]");
}

// Next function from function sequence. f_state should be initialized to one or
// zero. f should be initialized using f_init().
static inline bool f_next(size_t* f_state, int f[1 + BASE])
{
    size_t* i = f_state;
    if (f[1] >= BASE)
        return false;

    int d1, d2;
    if (f[*i] < BASE) { // flush from next
        ++*i;
    }
    else do { // flush from left
        --*i;
        d1 = f[*i-0] - f[*i-1];
        d2 = f[*i-1] - f[*i-2];
    } while (d1 == d2);

    int inc = f[*i] + 1;
    for (size_t j = *i; j < 1 + BASE; ++j) // flush
        f[j] = inc;
    return true;
}

// Filtering makes the edges of the function go crazy, this is length of
// extrapolation at the edges.
#define IIR_TAIL_LENGTH (IIR_POLES << 2)

// Both of these increase smoothing. IIR_INTENSITY also lowers precision, so
// shouldn't be too high. Higher IIR_POLES is more expensive, also should be
// modest. More smoothing, better quality second derivative, but too much
// smoothing make all functions the same.
#define IIR_INTENSITY 2
#define IIR_POLES 4

// Scale to fixed width and smooth out kinks with IIR filter.
static inline void f_filter(fixed_t f_out[restrict], const int f_in[restrict BASE])
{
    fixed_t f_right_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    fixed_t* f_right = f_right_mem + IIR_TAIL_LENGTH + BASE;

    for (size_t i = 0; i <= BASE; ++i) // copy and scale positive side
        f_out[i] = f_right[i] = f_in[i] << FIXED_WIDTH;
    for (size_t i = BASE; i < 1 + BASE + IIR_TAIL_LENGTH; ++i) // extrapolate
        f_out[i] = f_right[i] = f_out[BASE];
    for (size_t i = 1; i < 1 + BASE + IIR_TAIL_LENGTH; ++i) // mirror negative side
        f_out[-i] = f_right[-i] = -f_out[i];

    for (size_t k = 0; k < IIR_POLES; ++k) {
        // IIR filtering from right and left
        for (int i = 1 + BASE + IIR_TAIL_LENGTH - 2; i >= -BASE - IIR_TAIL_LENGTH; --i)
            f_right[i] = (f_right[i]>>IIR_INTENSITY) + (f_right[i + 1]>>1);
        for (int i = -1 - BASE - IIR_TAIL_LENGTH + 1; i <= BASE + IIR_TAIL_LENGTH; ++i)
            f_out[i] = (f_out[i]>>IIR_INTENSITY) + (f_out[i - 1]>>1);

        // Combine for zero phase
        for (int i = -BASE - IIR_TAIL_LENGTH; i <= BASE + IIR_TAIL_LENGTH; ++i)
            f_out[i] += f_right[i];
    }
}

// Call f with x with normalized scale, so f_call(f, 1.f) == f[BASE]. In between
// index values will be linearly interpolated.
static inline fixed_t f_call(const fixed_t f[restrict], float x)
{
    float floorf(float);
    float bx = BASE*x;
    float floor = floorf(bx);
    float fract = bx - floor;
    int i = floor;
    if (i >= BASE)
        return f[BASE];
    else if (i < -BASE)
        return f[-BASE];
    else
        return (1.f-fract)*f[i] + fract*f[i+1];
}


// Compare floating point numbers with given precision.
static inline bool is_equal_float(float a, float b, float max_relative_diff)
{
    float fabsf(float);
    float fmaxf(float, float);
    a = fabsf(a);
    b = fabsf(b);
    return fabsf(a - b) < max_relative_diff * fmaxf(a, b);
}

// Compare fixed point numbers with given precision.
static inline bool is_equal_fixed(fixed_t a, fixed_t b, fixed_t max_relative_diff)
{
    if (a < 0)
        a = -a;
    if (b < 0)
        b = -b;
    fixed_t abs_ab = a - b < 0 ? b - a : a - b;
    fixed_t max_ab = a > b ? a : b;
    return (int64_t)abs_ab << FIXED_WIDTH < (int64_t)max_relative_diff * max_ab;
}

// Time since epoch in nanoseconds.
__attribute__((always_inline)) static inline __uint128_t time_begin()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (__uint128_t)1000000000*ts.tv_sec + ts.tv_nsec;
}

// Time since start_ns in seconds, where start_ns is in nanoseconds.
__attribute__((always_inline)) static inline double time_diff(__uint128_t start_ns)
{
    return (double)(uint64_t)(time_begin() - start_ns) / 1000000000.;
}

// The quantile function of a Gaussian. p should be in range (0, 1).
float probitf(float p);

// Returns an input gain such that passing sine to f will yield a THD of
// THD_NORMALIZED. Compile with thd.c.
float normalized_input_gain(const fixed_t f[1 + BASE]);

// Returns an output gain such that passing Gaussian noise to f will yield some
// normalized RMS value.
static inline float normalized_output_gain(const fixed_t f[restrict], float input_gain)
{
    float sqrtf(float);
    int64_t sum = 0; // of squares
    const float dt = .5f/BASE;
    for (float t = dt; t < 1.f; t += dt) {
        int64_t x = f_call(f, input_gain*probitf(t));
        sum += x*x;
    }
    sum >>= FIXED_WIDTH;
    return 1.f / sqrtf(dt*(float)sum);
}

#endif // SHARED_H_INCLUDED
