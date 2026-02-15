#include "shared.h"
#include <math.h>

#define T_NOISE (1 << 16)

// Random number generator.
typedef struct rng
{
    uint64_t state;
    uint64_t inc;
} RNG;

// Create initialized random number state.
RNG rng_state(uint64_t init_state, uint64_t stream_id)
{
    uint32_t rng_next(RNG*);
    RNG rng = {.inc = (stream_id << 1u) | 1u };
    rng_next(&rng);
    rng.state += init_state;
    rng_next(&rng);
    return rng;
}

// Next random integer number with uniform distribution.
uint32_t rng_next(RNG* rng)
{
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((0-rot) & 31));
}

// Next random floating point number with uniform distribution of range [0, 1).
double rng_next_float(RNG* rng)
{
    return ldexp(rng_next(rng), -32);
}

// Next random floating point number with Gaussian distribution of range (∞, ∞).
double rng_gaussian(RNG* rng)
{
    double u1 = rng_next_float(rng);
    double u2 = rng_next_float(rng);
    return sqrt(-2.*log(u1)) * cos(2.*M_PI*u2);
}

int main(void)
{
    RNG rng = rng_state(time(NULL), (uintptr_t)&rng);
    float noise[T_NOISE];
    for (size_t t = 0; t < T_NOISE; ++t)
        noise[t] = rng_gaussian(&rng);

    float  blunter_mem[BASE + 1 + BASE];
    float  hard_clipper_mem[BASE + 1 + BASE];
    float  logistic_mem[BASE + 1 + BASE];
    float* blunter      = blunter_mem      + BASE;
    float* hard_clipper = hard_clipper_mem + BASE;
    float* logistic     = logistic_mem     + BASE;
    for (int i = -BASE; i <= BASE; ++i) {
        double x = (i+.5) / BASE;
        blunter[i]      = 2.*x - fabs(x)*x;
        hard_clipper[i] = fabs(x) < .5 ? x : .5*(x>0?1:-1);
        logistic[i]     = 2. / (1. + exp(-3.*x)) - 1.;
    }

    float blunter_input_gain       = normalized_input_gain(blunter);
    float hard_clipper_input_gain  = normalized_input_gain(hard_clipper);
    float logistic_input_gain      = normalized_input_gain(logistic);
    float blunter_output_gain      = normalized_output_gain(blunter, blunter_input_gain);
    float hard_clipper_output_gain = normalized_output_gain(hard_clipper, hard_clipper_input_gain);
    float logistic_output_gain     = normalized_output_gain(logistic, logistic_input_gain);

    float blunter_rms      = 0.f;
    float hard_clipper_rms = 0.f;
    float logistic_rms     = 0.f;

    for (size_t t = 0; t < T_NOISE; ++t) {
        float x;
        x = blunter_output_gain * f_call(blunter, blunter_input_gain*noise[t]);
        blunter_rms += x*x;
        x = hard_clipper_output_gain * f_call(hard_clipper, hard_clipper_input_gain*noise[t]);
        hard_clipper_rms += x*x;
        x = logistic_output_gain * f_call(logistic, logistic_input_gain*noise[t]);
        logistic_rms += x*x;
    }
    blunter_rms      = sqrtf(blunter_rms      / T_NOISE);
    hard_clipper_rms = sqrtf(hard_clipper_rms / T_NOISE);
    logistic_rms     = sqrtf(logistic_rms     / T_NOISE);
    const float min_expected_rms = 1.f; // probably should be much bigger
    assert(blunter_rms      > min_expected_rms);
    assert(hard_clipper_rms > min_expected_rms);
    assert(logistic_rms     > min_expected_rms);

    const float max_relative_diff = .01f;
    bool equal0 = is_equal_float(blunter_rms, hard_clipper_rms,  max_relative_diff);
    bool equal1 = is_equal_float(blunter_rms, logistic_rms,      max_relative_diff);
    bool equal2 = is_equal_float(hard_clipper_rms, logistic_rms, max_relative_diff);
    if (!equal0 || !equal1 || !equal2) {
        fprintf(stderr, "Output gain normalization [FAILED]\n");
        fprintf(stderr, "Blunter RMS:      %g\n", blunter_rms);
        fprintf(stderr, "Hard clipper RMS: %g\n", hard_clipper_rms);
        fprintf(stderr, "Logistic RMS:     %g\n", logistic_rms);
        exit(EXIT_FAILURE);
    }
    printf("[PASSED] RMS test.\n");
}
