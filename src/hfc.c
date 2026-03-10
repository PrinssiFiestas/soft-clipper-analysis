#include "shared.h"
extern const float sine[T];
extern fixed_t sines[T/4][T];

// High frequency content (HFC) of f.
float f_hfc(float f[], float out_gain, float in_gain)
{
    fixed_t x[T];
    for (size_t t = 0; t < T; ++t)
        x[t] = A*f_call(f, in_gain*sine[t]);

    fixed_t bs[T/4];
    for (size_t k = 0; k < T/4 - SKIP; ++k) {
        int64_t b = 0;
        for (size_t t = 0; t < T; ++t)
            b += (int64_t)x[t] * sines[k][t];
        bs[k] = b >> FIXED_WIDTH;
    }

    float hfc = 0.f;
    for (size_t i = 0; i < T/4 - SKIP; ++i)
        hfc += out_gain*i*fabsf((float)bs[i]/A);

    return hfc;
}

// Maximum of change of high frequency content (HFC) of f.
float f_hfc_max_change(float f[], float out_gain, float in_gain)
{
    float hfc_diff_max = 0.f;
    const float da = 1.f/32.f;
    const float smoothing = .99f;
    float hfc1 = f_hfc(f, out_gain, da*in_gain);
    float hfc_diff1 = 0.f;

    for (float a = 2.f*da; a <= 1.0001f; a += da) {
        float hfc = f_hfc(f, out_gain, a*in_gain);
        float hfc_diff = (1.f - smoothing)*fabsf(hfc - hfc1) + smoothing*hfc_diff1;
        if (hfc_diff > hfc_diff_max)
            hfc_diff_max = hfc_diff;
        hfc1 = hfc;
        hfc_diff1 = hfc_diff;
    }
    return hfc_diff_max/da;
}

#ifdef HFC_MAIN
int main(void)
{
    int f_gen[1 + BASE];
    uint32_t f_state = 1;
    f_init(f_gen);
    float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* f = f_mem + IIR_TAIL_LENGTH + BASE;

    size_t skip = 0;
    do {
        if (skip++ & 3) // less busy scatter plot and increased computation speed.
            continue;

        f_filter(f, f_gen);
        float in_gain  = normalized_input_gain(f);
        float out_gain = normalized_output_gain(f, in_gain);
        float hardness = f_hardness(f, out_gain, in_gain);
        if (hardness > 32.f)
            continue;

        #if 0 // HFC
        printf("%f, %f\n", hardness, f_hfc(f, out_gain, in_gain));
        #else // change in HFC
        printf("%f, %f\n", hardness, f_hfc_max_change(f, out_gain, in_gain));
        #endif
    } while (f_next(&f_state, f_gen));
}
#endif // HFC_MAIN
