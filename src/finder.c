#include "shared.h"

#define OVERSAMPLE_POWER 4

#ifdef CUSTOM
// TODO
#error CUSTOM not implemented yet, sorry.
#endif

static float debug_buf1[1 + BASE];
static float debug_buf2[1 + BASE];

// Derivative to be inspected in debugger.
float* debug_derivative(size_t length, float f[])
{
    for (size_t i = 1; i < length; ++i)
        debug_buf1[i] = (length) * (f[i] - f[i - 1]);
    debug_buf1[0] = debug_buf1[1];
    return debug_buf1;
}

// Second derivative to be inspected in debugger.
float* debug_derivative2(size_t length, float f[])
{
    debug_derivative(length, f);
    for (size_t i = 1; i <= length; ++i)
        debug_buf2[i] = length * (debug_buf1[i] - debug_buf1[i - 1]);
    debug_buf2[0] = debug_buf2[1] = 0.f;
    return debug_buf2;
}

float* oversampled_derivative(
    float f_oversampled[restrict (IIR_TAIL_LENGTH + 1 + BASE) << OVERSAMPLE_POWER],
    float f_in[restrict])
{
    float f_buf_mem[(IIR_TAIL_LENGTH + 1 + BASE) << OVERSAMPLE_POWER];
    float* f_buf = f_buf_mem;

    // Derivative. Need data in front for IIR filtering. Also, an extra point
    // at the end prevent off by one when linearly interpolating later.
    for (int i = -IIR_TAIL_LENGTH; i <= BASE + 1; ++i) {
        int j = i + IIR_TAIL_LENGTH;
        f_buf[j] = f_in[i] - f_in[i - 1];
    }

    for (size_t pow = 0; pow < OVERSAMPLE_POWER; ++pow)
    {
        // TODO should do bidirectional filtering. We cannot move the x axis,
        // this would yield to discarding ranges of values!

        // Expand by linearly interpolating.
        for (size_t i = 0; i < (IIR_TAIL_LENGTH + 1lu + BASE) << pow; ++i) {
            f_oversampled[2*i + 0] = f_buf[i];
            f_oversampled[2*i + 1] = .5f * (f_buf[i] + f_buf[i + 1]);
        }

        // IIR smooth out linear interpolation distortion.
        float b = .33f;
        float a = (1.f - b);
        for (size_t i = 1; i < (IIR_TAIL_LENGTH + 1lu + BASE) << (pow + 1); ++i)
            f_oversampled[i] = a*f_oversampled[i] + b*f_oversampled[i-1];

        float* swap = f_oversampled;
        f_oversampled = f_buf;
        f_buf = swap;
    }

    // Unswap
    if ((OVERSAMPLE_POWER & 1) == 0)
        memcpy(f_oversampled, f_buf, sizeof f_buf_mem);
    else
        f_oversampled = f_buf;

    return f_oversampled + (IIR_TAIL_LENGTH << OVERSAMPLE_POWER);
}

int main(void)
{
    #if TEST_LINEAR_GAIN // The closer the generated function is to a linear
                         // function, the more insane the in gains are, so we
                         // need to discard functions with too much input gain.
    int    linear_gen[1 + BASE];
    float  linear_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* linear = linear_mem + IIR_TAIL_LENGTH + BASE;

    for (int i = 0; i <= BASE; ++i)
        linear_gen[i] = i;
    // linear_gen[BASE] = linear_gen[BASE-1]; // almost linear
    f_filter(linear, linear_gen);
    float linear_in_gain = normalized_input_gain(linear);
    printf("%f\n", linear_in_gain);
    exit(0);
    #endif // TEST_LINEAR_GAIN

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
    float  blunter_diff_buf[1 + BASE]  = {0};
    float  arctan_diff_buf[1 + BASE]   = {0};
    float  logistic_diff_buf[1 + BASE] = {0};
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
    float  blunter_gen_input_gain   = 0.f;
    float  blunter_gen_output_gain  = 0.f;
    float  arctan_gen_input_gain    = 0.f;
    float  arctan_gen_output_gain   = 0.f;
    float  logistic_gen_input_gain  = 0.f;
    float  logistic_gen_output_gain = 0.f;

    float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* f = f_mem + IIR_TAIL_LENGTH + BASE;
    int f_gen[1 + BASE];
    uint32_t f_gen_state = 1;
    f_init(f_gen);

    size_t index = 0;
    do { // find functions in generated sequences
        f_filter(f, f_gen);
        float f_input_gain = normalized_input_gain(f);
        float f_output_gain = normalized_output_gain(f, f_input_gain);

        float diff;
        float diff_buf[1 + BASE];

        diff = 0.f;
        for (size_t i = 0; i < 1 + BASE; ++i) {
            float x = (float)i / (1 + BASE);
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
        for (size_t i = 0; i < 1 + BASE; ++i) {
            float x = (float)i / (1 + BASE);
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
        for (size_t i = 0; i < 1 + BASE; ++i) {
            float x = (float)i / (1 + BASE);
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

    float blunter_normalized[1 + BASE];
    float arctan_normalized[1 + BASE];
    float logistic_normalized[1 + BASE];
    for (size_t i = 0; i < 1 + BASE; ++i) {
        float x = (float)i / (1 + BASE);
        blunter_normalized[i] = blunter_gen_output_gain * f_call(
            blunter_gen_filtered, blunter_gen_input_gain*x);
        arctan_normalized[i] = arctan_gen_output_gain * f_call(
            arctan_gen_filtered, arctan_gen_input_gain*x);
        logistic_normalized[i] = logistic_gen_output_gain * f_call(
            logistic_gen_filtered, logistic_gen_input_gain*x);
    }

    const float plot_scale  = 1.f;
    float blunter_diff_sum  = 0.f;
    float arctan_diff_sum   = 0.f;
    float logistic_diff_sum = 0.f;
    size_t start = 0;
    printf("%i, %f\n", -1, plot_scale);
    for (size_t i = start; i < 1 + BASE; ++i) {
        blunter_diff_sum += blunter_diff_buf[i];
        printf("%zu, %f\n", i, blunter_diff_buf[i]);
    }
    printf("%i, %f\n", 1 + BASE, blunter_diff_sum);
    for (size_t i = start; i < 1 + BASE; ++i) {
        arctan_diff_sum += arctan_diff_buf[i];
        printf("%zu, %f\n", i, arctan_diff_buf[i]);
    }
    printf("%i, %f\n", 1 + BASE, arctan_diff_sum);
    for (size_t i = start; i < 1 + BASE; ++i) {
        logistic_diff_sum += logistic_diff_buf[i];
        printf("%zu, %f\n", i, logistic_diff_buf[i]);
    }
    printf("%i, %f\n", 1 + BASE, logistic_diff_sum);
    printf("%i, %f\n", 1 + BASE+1,
           (blunter_diff_sum + arctan_diff_sum + logistic_diff_sum)/3);

    // Values for debugging.
    (void)blunter_index;
    (void)arctan_index;
    (void)logistic_index;
    (void)blunter_normalized;
    (void)arctan_normalized;
    (void)logistic_normalized;

    float* blunter_derivative = oversampled_derivative(
        (float[(IIR_TAIL_LENGTH + 1 + BASE + IIR_TAIL_LENGTH) << OVERSAMPLE_POWER]){0},
        blunter_gen_filtered);
    float* arctan_derivative = oversampled_derivative(
        (float[(IIR_TAIL_LENGTH + 1 + BASE) << OVERSAMPLE_POWER]){0},
        arctan_gen_filtered);
    float* logistic_derivative = oversampled_derivative(
        (float[(IIR_TAIL_LENGTH + 1 + BASE) << OVERSAMPLE_POWER]){0},
        logistic_gen_filtered);

    (void)blunter_derivative;
    (void)arctan_derivative;
    (void)logistic_derivative;
}
