#ifndef SHARED_H_INCLUDED
#define SHARED_H_INCLUDED 1

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

// Fixed point type.
typedef int fixed_t;

#ifndef BASE // defined in Makefile, this is here just for clangd.
// Generator number base. This will also determine the precision of the
// generated clipping function.
#define BASE 48
#endif

#define STR(S) #S
#define XSTR(S) STR(S)
#define BASE_STR XSTR(BASE)

#if HFC
#define HFC_STR "HFC"
#else
#define HFC_STR ""
#endif

// Number of samples. Should be at least 2*BASE to fit a full sinusoid.
// Bigger makes DFT much more accurate, but clearly needs more processing time.
// Does not need to be a power of two, we use DFT instead of FFT.
#define T (7*BASE)

// Fixed point fractional bits. 24 is guaranteed to overflow, so keep it sane.
#define FIXED_WIDTH 20

// Amplitude of sines or fixed width precision.
#define A (1<<FIXED_WIDTH)

// We'll normalize against Blunter's THD by default.
#ifndef THD_NORMALIZED
#define THD_NORMALIZED 0.0222559f
#endif

#define SKIP 2 // harmonics close to Nyquist are likely to be dominated by noise.

// Filtering makes the edges of the function go crazy, this is length of
// extrapolation at the edges.
#define IIR_TAIL_LENGTH 8

// Both of these increase smoothing. IIR_INTENSITY also lowers precision, so
// shouldn't be too high. Higher IIR_POLES is more expensive, also should be
// modest. More smoothing, better quality second derivative, but too much
// smoothing make all functions the same.
#define IIR_INTENSITY 1
#define IIR_POLES 3
// Good combinations: (1, 3)

// Values bigger than one need extrapolation. Too much extrapolation will be
// unreliable and must be discarded.
#define MAX_IN_GAIN 1.5f

#ifndef WORK_SIZE
// Sequence length for each thread.
#define WORK_SIZE (1 << 16)
#endif

#ifndef GPU_WORK_SIZE
// Sequence length for each GPU work unit. Should be small to avoid GPU hang and
// branching.
#define GPU_WORK_SIZE (1 << 0)
#endif

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#ifndef NPROC
#define NPROC 8
#endif

#define WORK_GROUP_SIZE 16

// Can be defined to store and inspect extra data from shader.
//#define GPU_DEBUG 1

typedef struct work
{
    _Alignas(CACHE_LINE_SIZE)
    uint64_t f_index;
    uint32_t f_state;
    float    f_hardness;
    int      f_gen[1 + BASE];

    #if GPU_DEBUG
    float value1;
    float value2;
    #endif
} Work;

typedef _Atomic enum worker_state
{
    WAITING,
    BUSY,
    GOT_RESULT,
} Worker;

// Initialize clipper function generator.
static inline void f_init(int f[1 + BASE])
{
    // All zeros doesn't make sense, so start with step to one. Starting from a
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

// Checks if generated function is increasing and it's derivative is decreasing.
static inline bool f_valid(const int f[1 + BASE])
{
    bool value_increasing = true;
    bool diff_decreasing  = true;
    int  diff = f[1];

    for (size_t i = 1; i < 1 + BASE; ++i) {
        value_increasing = f[i] >= f[i-1];
        diff_decreasing  = f[i] - f[i-1] <= diff;
        if (!value_increasing || !diff_decreasing)
            return false;
        diff = f[i] - f[i-1];
    }

    return true;
}

// Next function from function sequence. f_state should be initialized to one or
// zero. f should be initialized using f_init().
static inline bool f_next(uint32_t* f_state, int f[1 + BASE])
{
    uint32_t* i = f_state;
    if (f[1] >= BASE)
        return false;

    int d1, d2;
    if (f[*i] < BASE) { // flush from next
        ++*i;
    }
    else do { // find from where to flush
        --*i;
        d1 = f[*i-0] - f[*i-1];
        d2 = f[*i-1] - f[*i-1-(*i>1)];
    } while (d1 == d2);

    int inc = f[*i] + 1;
    for (size_t j = *i; j < 1 + BASE; ++j) // flush
        f[j] = inc;
    return true;
}

// Smooth out kinks with IIR filter.
static inline void f_filter(float f_out[restrict], const int f_in[restrict BASE])
{
    float f_right_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* f_right = f_right_mem + IIR_TAIL_LENGTH + BASE;

     // Copy positive side.
    for (size_t i = 0; i <= BASE; ++i)
        f_out[i] = f_right[i] = f_in[i];

    // Find first and second derivative for better extrapolation.
    float d1 = 0.f;
    float d2 = 0.f;
    for (int i1 = BASE - 1; i1 >= 0; --i1) {
        if (f_in[i1] != f_in[BASE]) {
            d1 = (float)(f_in[BASE] - f_in[i1]) / (BASE - i1);
            for (int i2 = i1 - 1; i2 >= 0; --i2) {
                if (f_in[i2] != f_in[i1]) {
                    float _d2 = (float)(f_in[i1] - f_in[i2]) / (i1 - i2);
                    d2 = (d1 -_d2) / (i1 - i2);
                    break;
                }
            }
            break;
        }
    }
    float extra = f_out[BASE];
    for (size_t i = BASE + 1; i < 1 + BASE + IIR_TAIL_LENGTH; ++i) { // extrapolate
        extra += d1;
        d1 += d2;
        if (d1 < 0.f)
            d1 = d2 = 0;
        f_out[i] = f_right[i] = extra;
    }

    // Mirror negative side.
    for (size_t i = 1; i < 1 + BASE + IIR_TAIL_LENGTH; ++i)
        f_out[-i] = f_right[-i] = -f_out[i];

    for (size_t k = 0; k < IIR_POLES; ++k) {
        // IIR filtering from right and left
        const float a = 1.f / (1<<IIR_INTENSITY);
        const float b = 1.f - a;
        for (int i = 1 + BASE + IIR_TAIL_LENGTH - 2; i >= -BASE - IIR_TAIL_LENGTH; --i)
            f_right[i] = a*f_right[i] + b*f_right[i + 1];
        for (int i = -1 - BASE - IIR_TAIL_LENGTH + 2; i <= BASE + IIR_TAIL_LENGTH; ++i)
            f_out[i] = a*f_out[i] + b*f_out[i - 1];
    }
    // Combine for zero phase. Scale as well for sensible ranges.
    for (int i = -BASE - IIR_TAIL_LENGTH; i <= BASE + IIR_TAIL_LENGTH; ++i)
        f_out[i] = (1.f/BASE) * (f_out[i] + f_right[i]);
}

// Call f with x with normalized scale, so f_call(f, 1.f) == f[BASE]. In between
// index values will be linearly interpolated, out of bounds values will be
// quadratically extrapolated.
static inline float f_call(const float f[restrict], float x)
{
    float t = BASE*x;
    float floor = floorf(t);
    float fract = t - floor;
    int i = (int)floor >= INT_MAX ? INT_MAX : (int)floor < INT_MIN ? INT_MIN : floor;

    if (-BASE <= i && i < BASE) // interpolate
        return (1.f-fract)*f[i] + fract*f[i+1];
    // else extrapolate.

    float d1 = f[BASE] - f[BASE - 1];            // first derivative
    float d2 = d1 - (f[BASE - 1] - f[BASE - 2]); // second derivative

    if (d1 <= 0.f)
        return i >= BASE ? f[BASE] : f[-BASE];

    // Quadratic polynomial P(x) = a*x^2 + b*x + c. Coefficients a and b can be
    // solved from derivatives P''(BASE) = 2*a = d2
    // and P'(BASE) = 2*a*BASE + b = d1. c can be solved from P(BASE) = f(BASE).
    float a = .5f*d2;
    float b = d1 - 2.f*a*(BASE-1.f);
    float c = f[BASE] - a*BASE*BASE - b*BASE;

    if (a == 0.f) // avoid zero division
        return b*t + c;

    // Clamp at the top of P(x), which can be found using P'(x_max) = 0.
    float absx = fabsf(t);
    float x_max = -b/(2.f*a);
    if (absx >= x_max) {
        float y = a*x_max*x_max + b*x_max + c;
        return t >= 0.f ? y : -y;
    }
    float y = a*absx*absx + b*absx + c;
    return t >= 0.f ? y : -y;
}

// Monotone cubic interpolation of values y0 and y1 using x as the interpolation
// parameter (assumed to be [0..1]). A modification of hermite cubic interpolation
// that prevents overshoots (preserves monoticity). In order to both maintain
// monotonicity and C1 continuity, two neighbouring samples to the left of y0 and
// the right of y1 are also necessary
float interpolate_cubic_monotonic_heckbert(
    float x,
    float y_minus_1,
    float y0,
    float y1,
    float y2);

// Like f_call(), but uses monotone cubic Hermite interpolation for in between
// values instead of linear interpolation.
static inline float f_call_cubic(const float f[restrict], float x)
{
    if (fabsf(x) > BASE) // extrapolate
        return f_call(f, x);
    // else interpolate.

    float t = BASE*x;
    float floor = floorf(t);
    float fract = t - floor;
    int i = floor;
    float y_minus_1 = -BASE <= i - 1 ? f[i - 1] : f_call(f, x - 1.f/BASE);
    float y0 = f[i];
    float y1 = i + 1 <= BASE ? f[i + 1] : f_call(f, x + 1.f/BASE);
    float y2 = i + 2 <= BASE ? f[i + 2] : f_call(f, x + 1.f/BASE);
    return interpolate_cubic_monotonic_heckbert(
        fract, y_minus_1, y0, y1, y2);
}

// Compare floating point numbers with given precision.
static inline bool is_equal_float(float a, float b, float max_relative_diff)
{
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

#define TIME_STR_BUF_SIZE (sizeof"2147483647 years, 11 months, 29 days")

// Used for rough time estimations, conversion not precise.
static inline char* time_str(char buf[TIME_STR_BUF_SIZE], double t)
{
    if (isnan(t) || t < 0)
        return NULL;
    if (t/(60*60*24*30*12) > INT_MAX) {
        sprintf(buf, "infinite years");
        return buf;
    }
    int ts = fmod(t/(1),           60.);
    int tm = fmod(t/(60),          60.);
    int th = fmod(t/(60*60),       24.);
    int td = fmod(t/(60*60*24),    30.);
    int tM = fmod(t/(60*60*24*30), 12.);
    int ty =      t/(60*60*24*30*12)   ;
    if (t < 60)
        sprintf(buf, "%i seconds", ts);
    else if (t < 60*60)
        sprintf(buf, "%i minutes, %i seconds", tm, ts);
    else if (t < 60*60*24)
        sprintf(buf, "%i hours, %i minutes, %i seconds", th, tm, ts);
    else if (t < 60*60*24*30)
        sprintf(buf, "%i days, %i hours, %i minutes", td, th, tm);
    else if (t < 60*60*24*30*12)
        sprintf(buf, "%i months, %i days, %i hours", tM, td, th);
    else
        sprintf(buf, "%i years, %i months, %i days", ty, tM, td);
    return buf;
}

// Stores current date to buf and returns it or returns NULL in case of failure.
static inline char* get_date(char buf[70])
{
    time_t timer = time(NULL);
    if (strftime(buf, 70, "%c", localtime(&timer)) > 0)
        return buf;
    return NULL;
}

// The quantile function of a Gaussian. p should be in range (0, 1).
float probitf(float p);

// Returns an input gain such that passing sine to f will yield a THD of
// THD_NORMALIZED. Compile with thd.c.
float normalized_input_gain(const float f[1 + BASE]);

// Returns an output gain such that passing Gaussian noise to f will yield some
// normalized RMS value.
static inline float normalized_output_gain(const float f[restrict], float input_gain)
{
    float sum = 0.f; // of squares
    const float dt = .5f/BASE;
    for (float t = dt; t < 1.f; t += dt) {
        float x = f_call(f, input_gain*probitf(t));
        sum += x*x;
    }
    return 1.f / sqrtf(dt*sum);
}

// Returns hardness of clipping function.
static inline float f_hardness(const float f[restrict], float out_gain, float in_gain)
{
    // Check if too much data out of bounds for reliable results. Experiment
    // showed that most input gains are well below 1 anyway.
    if (in_gain > MAX_IN_GAIN) // don't try to extrapolate.
        return 1e20f; // discard

    // If f_normalized(x) = out_gain*f(in_gain*x), then chain rule gives us
    // f_normalized''(x) = out_gain*in_gain^2*f''(in_gain*x). We don't need to
    // care about x, we just need the minimum value of the second derivative.
    float min = 0.f;
    for (size_t i = 0; i <= BASE; ++i) {
        float d0 = f[i-0] - f[i-1];
        float d1 = f[i+1] - f[i+0];
        min = fminf(min, d1 - d0);
    }
    float hardness = -out_gain * in_gain * in_gain * min * BASE * BASE;
    if (hardness <= .1) // impossible, bug somewhere
        return 1e20;
    if (in_gain <= 1.f) // all data included, can trust result.
        return hardness;
    // else check if we have to extrapolate.

    // We need some estimate if the min would fall. The problem is that our
    // second derivative is already very noisy, so we'll average out some range
    // at the end to estimate the trend.
    float third_derivative_sum  = 0.f;
    for (size_t i = 2*BASE/3; i <= BASE; ++i) {
        float d0 = f[i-0] - f[i-1];
        float d1 = f[i+1] - f[i+0];
        float d2 = f[i+2] - f[i+1];
        third_derivative_sum += (d2 - d1) - (d1 - d0);
    }
    if (third_derivative_sum >= 0.f) // min not likely to change, can trust result.
        return hardness;
    // else safer to just discard.
    return 1e20f;
}

static float debug_buf1[1 + BASE];
static float debug_buf2[1 + BASE];

// Derivative to be inspected in debugger.
__attribute__((unused)) static float* debug_derivative(size_t length, float f[])
{
    for (size_t i = 1; i < length; ++i)
        debug_buf1[i] = (length) * (f[i] - f[i - 1]);
    debug_buf1[0] = debug_buf1[1];
    return debug_buf1;
}

// Second derivative to be inspected in debugger.
__attribute__((unused)) static float* debug_derivative2(size_t length, float f[])
{
    debug_derivative(length, f);
    for (size_t i = 1; i <= length; ++i)
        debug_buf2[i] = length * (debug_buf1[i] - debug_buf1[i - 1]);
    debug_buf2[0] = debug_buf2[1] = 0.f;
    return debug_buf2;
}

extern bool g_got_gpu;
// NOTE: OpenGL context is thread local! Only call this in GPU thread!
bool gpu_init(void);
void gpu_compute(size_t buffer_length, size_t buffer_element_size, void* buffer);
Work gpu_do_work(const Work* work);
void gpu_destroy(void);

// Maximum of change of high frequency content (HFC) of f.
float f_hfc_max_change(float f[], float out_gain, float in_gain);

#endif // SHARED_H_INCLUDED
