#include "shared.h"
#include <limits.h>
#include <math.h>

// TODO X macro and #ifdef CUSTOM

#define DIFF_BUF_LENGTH (2*(1 + BASE))

int main(void)
{
    float  blunter_mem[BASE + 1 + BASE];
    float  arctan_mem[BASE + 1 + BASE];
    float  logistic_mem[BASE + 1 + BASE];
    float* blunter  = blunter_mem  + BASE;
    float* arctan   = arctan_mem   + BASE;
    float* logistic = logistic_mem + BASE;
    for (int i = -BASE; i <= BASE; ++i) {
        double x = (float)i / BASE;
        blunter[i]  = 2.*x - fabs(x)*x;
        arctan[i]   = atan(2.*x);
        logistic[i] = 2. / (1. + exp(-3.*x)) - 1.;
    }
    float blunter_input_gain   = normalized_input_gain(blunter);
    float arctan_input_gain    = normalized_input_gain(arctan);
    float logistic_input_gain  = normalized_input_gain(logistic);
    float blunter_output_gain  = normalized_output_gain(blunter, blunter_input_gain);
    float arctan_output_gain   = normalized_output_gain(arctan, arctan_input_gain);
    float logistic_output_gain = normalized_output_gain(logistic, logistic_input_gain);

    float  blunter_min_diff  = INFINITY;
    float  arctan_min_diff   = INFINITY;
    float  logistic_min_diff = INFINITY;
    float  blunter_diff_buf[DIFF_BUF_LENGTH]  = {0};
    float  arctan_diff_buf[DIFF_BUF_LENGTH]   = {0};
    float  logistic_diff_buf[DIFF_BUF_LENGTH] = {0};
    size_t blunter_index  = 0;
    size_t arctan_index   = 0;
    size_t logistic_index = 0;
    int    blunter_gen[1 + BASE]  = {0};
    int    arctan_gen[1 + BASE]   = {0};
    int    logistic_gen[1 + BASE] = {0};
    float  blunter_gen_filtered_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH]  = {0};
    float  arctan_gen_filtered_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH]   = {0};
    float  logistic_gen_filtered_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH] = {0};
    float* blunter_gen_filtered  = blunter_gen_filtered_mem  + IIR_TAIL_LENGTH + BASE;
    float* arctan_gen_filtered   = arctan_gen_filtered_mem   + IIR_TAIL_LENGTH + BASE;
    float* logistic_gen_filtered = logistic_gen_filtered_mem + IIR_TAIL_LENGTH + BASE;
    float  blunter_gen_input_gain  = 0.f;
    float  blunter_gen_output_gain = 0.f;
    float  arctan_gen_input_gain  = 0.f;
    float  arctan_gen_output_gain = 0.f;
    float  logistic_gen_input_gain  = 0.f;
    float  logistic_gen_output_gain = 0.f;

    float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* f = f_mem + IIR_TAIL_LENGTH + BASE;
    int f_gen[1 + BASE];
    size_t f_gen_state = 1;
    f_init(f_gen);

    size_t index = 0;
    do { // find functions in generated sequences
        f_filter(f, f_gen);
        float f_input_gain = normalized_input_gain(f);
        float f_output_gain = normalized_output_gain(f, f_input_gain);

        float diff;
        float diff_buf[DIFF_BUF_LENGTH];

        diff = 0.f;
        for (size_t i = 0; i < DIFF_BUF_LENGTH; ++i) {
            float x = (float)i / DIFF_BUF_LENGTH;
            diff += diff_buf[i] = fabsf(
                blunter_output_gain*f_call(blunter, blunter_input_gain*x)
                    - f_output_gain*f_call(f, f_input_gain*x));
        }
        if (diff < blunter_min_diff) {
            blunter_index = index;
            blunter_min_diff = diff;
            memcpy(blunter_diff_buf, diff_buf, sizeof diff_buf);
            memcpy(blunter_gen_filtered_mem, f_mem, sizeof f_mem);
            memcpy(blunter_gen, f_gen, sizeof f_gen);
            blunter_gen_input_gain  = f_input_gain;
            blunter_gen_output_gain = f_output_gain;
        }

        diff = 0.f;
        for (size_t i = 0; i < DIFF_BUF_LENGTH; ++i) {
            float x = (float)i / DIFF_BUF_LENGTH;
            diff += diff_buf[i] = fabsf(
                arctan_output_gain*f_call(arctan, arctan_input_gain*x)
                    - f_output_gain*f_call(f, f_input_gain*x));
        }
        if (diff < arctan_min_diff) {
            arctan_index = index;
            arctan_min_diff = diff;
            memcpy(arctan_diff_buf, diff_buf, sizeof diff_buf);
            memcpy(arctan_gen_filtered_mem, f_mem, sizeof f_mem);
            memcpy(arctan_gen, f_gen, sizeof f_gen);
            arctan_gen_input_gain  = f_input_gain;
            arctan_gen_output_gain = f_output_gain;
        }

        diff = 0.f;
        for (size_t i = 0; i < DIFF_BUF_LENGTH; ++i) {
            float x = (float)i / DIFF_BUF_LENGTH;
            diff += diff_buf[i] = fabsf(
                logistic_output_gain*f_call(logistic, logistic_input_gain*x)
                    - f_output_gain*f_call(f, f_input_gain*x));
        }
        if (diff < logistic_min_diff) {
            logistic_index = index;
            logistic_min_diff = diff;
            memcpy(logistic_diff_buf, diff_buf, sizeof diff_buf);
            memcpy(logistic_gen_filtered_mem, f_mem, sizeof f_mem);
            memcpy(logistic_gen, f_gen, sizeof f_gen);
            logistic_gen_input_gain  = f_input_gain;
            logistic_gen_output_gain = f_output_gain;
        }
    } while (++index, f_next(&f_gen_state, f_gen));

    float blunter_normalized[DIFF_BUF_LENGTH];
    float arctan_normalized[DIFF_BUF_LENGTH];
    float logistic_normalized[DIFF_BUF_LENGTH];
    for (size_t i = 0; i < DIFF_BUF_LENGTH; ++i) {
        float x = (float)i / DIFF_BUF_LENGTH;
        blunter_normalized[i] = blunter_gen_output_gain * f_call(
            blunter_gen_filtered, blunter_gen_input_gain*x);
        arctan_normalized[i] = arctan_gen_output_gain * f_call(
            arctan_gen_filtered, arctan_gen_input_gain*x);
        logistic_normalized[i] = logistic_gen_output_gain * f_call(
            logistic_gen_filtered, logistic_gen_input_gain*x);
    }

    const float plot_scale  = .3f; // TODO change to 1.f
    float blunter_diff_sum  = 0.f;
    float arctan_diff_sum   = 0.f;
    float logistic_diff_sum = 0.f;
    printf("%i, %f\n", -1, plot_scale);
    for (size_t i = 0; i < DIFF_BUF_LENGTH; ++i) {
        blunter_diff_sum += blunter_diff_buf[i];
        printf("%zu, %f\n", i, blunter_diff_buf[i]);
    }
    printf("%i, %f\n", DIFF_BUF_LENGTH, blunter_diff_sum);
    for (size_t i = 0; i < DIFF_BUF_LENGTH; ++i) {
        arctan_diff_sum += arctan_diff_buf[i];
        printf("%zu, %f\n", i, arctan_diff_buf[i]);
    }
    printf("%i, %f\n", DIFF_BUF_LENGTH, arctan_diff_sum);
    for (size_t i = 0; i < DIFF_BUF_LENGTH; ++i) {
        logistic_diff_sum += logistic_diff_buf[i];
        printf("%zu, %f\n", i, logistic_diff_buf[i]);
    }
    printf("%i, %f\n", DIFF_BUF_LENGTH, logistic_diff_sum);
    printf("%i, %f\n", DIFF_BUF_LENGTH+1,
           (blunter_diff_sum + arctan_diff_sum + logistic_diff_sum)/3);

    // TODO report indices and print sequence?
    (void)blunter_index;
    (void)arctan_index;
    (void)logistic_index;
    (void)blunter_normalized;
    (void)arctan_normalized;
    (void)logistic_normalized;
}
