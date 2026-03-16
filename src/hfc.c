// WTHD (weighted total harmonic distortion) calculations. This started as HFC
// (high frequency content measure) calculation, so the naming is off. We mean
// WTHD when using HFC.

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

    float sum = 0.f;
    for (size_t i = 1; i < T/4 - SKIP; ++i)
        sum += (2*i+1) * ((float)bs[i]/A) * ((float)bs[i]/A);
    return sqrtf(sum) / ((float)bs[0]/A);
    (void)out_gain;
}

// Maximum of change of high frequency content (HFC) of f.
float f_hfc_max_change(float f[], float out_gain, float in_gain)
{
    float hfc_diff_max = 0.f;
    const float da = 1.f/32.f;
    #if 0 // filtering
    const float smoothing = .99f;
    #else
    const float smoothing = .0f;
    #endif

    float hfc1 = f_hfc(f, out_gain, da*in_gain);
    #if 1 // filtering bias
    float hfc_diff1 = -.8f * f_hfc(f, out_gain, da*in_gain);
    #else
    float hfc_diff1 = 0.f;
    #endif

    for (float a = 2.f*da; a <= 1.0001f; a += da) {
        float hfc = f_hfc(f, out_gain, a*in_gain);
        float hfc_diff = (1.f - smoothing)*(hfc - hfc1) + smoothing*hfc_diff1;
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
        if (skip++ & 63) // less busy scatter plot and increased computation speed.
            continue;

        f_filter(f, f_gen);
        float in_gain  = normalized_input_gain(f);
        float out_gain = normalized_output_gain(f, in_gain);
        float hardness = f_hardness(f, out_gain, in_gain);
        //if (hardness > 32.f)
        if (hardness > 256.f)
            continue;
        float hfc_hardness = f_hfc_max_change(f, out_gain, in_gain);
        printf("%f, %f\n", hardness, hfc_hardness);
    } while (f_next(&f_state, f_gen));
}
#endif // HFC_MAIN

float erff2(float x) { return erff(1.1f*x); }
float tanhf2(float x) { return tanhf(1.2f*x); }
float atanf2(float x) { return atanf(1.5f*x); }
float asinhf2(float x) { return asinhf(3.5f*x); }
float blunterf(float x) { return x > 1.f ? 1.f : x < -1.f ? -1.f : 2.f*x - fabsf(x)*x; }
float cubicf0(float x) { return x < -1.f ? -2.f/3.f : x > 1.f ? 2.f/3.f : x - x*x*x/3.f; }
float cubicf(float x) { return cubicf0(1.f*x); }
float sinusf(float x) { return x > 1.f ? 1.f : x < -1.f ? -1.f : sinf((M_PI/2.f) * x); }
float sigf0(float x) { return x / (1.f + fabsf(x)); }
float sigf(float x) { return sigf0(1.1f*x); }
float logisticf(float x) { return -2.f / (1.f + expf(2.3f*x)) + 1.f; }
float gdf(float x) { return 2.f * atanf(tanhf(x)); }
float algebraicf0(float x) { return x / sqrtf(1.f + x*x); }
float algebraicf(float x) { return algebraicf0(1.2f*x); }
float transitionf(float x) { return x > 1.f ? 1.f : x < -1.f ? -1.f : tanhf(2.f*x / (1.f - x*x)); }
float logarithmicf0(float x) { return (x<0?-1:1) * logf(fabsf(x) + 1.f) / logf(2.f); }
float logarithmicf(float x) { return logarithmicf0(3.8f*x); }

float softf(float x, float k, float t)
{
    float absx = fabsf(x);
    float px = -1.f/(4.f*k)*x*x + (1.f/2.f + t/(2.f*k))*absx + (-t*t/(4.f*k) + t/2.f - k/4.f);
    return absx < t - k ? x
        : (t - k <= absx && absx < t + k) ? (x<0?-1:1) * px
        : t*(x<0?-1:1);
}

float soft01(float x) { return softf(2.f*x, .01f, 1.f); }
float soft02(float x) { return softf(2.f*x, .02f, 1.f); }
float soft03(float x) { return softf(2.f*x, .03f, 1.f); }
float soft04(float x) { return softf(2.f*x, .04f, 1.f); }
float soft05(float x) { return softf(2.f*x, .05f, 1.f); }
float soft06(float x) { return softf(2.f*x, .06f, 1.f); }
float soft07(float x) { return softf(2.f*x, .07f, 1.f); }
float soft08(float x) { return softf(2.f*x, .08f, 1.f); }
float soft09(float x) { return softf(2.f*x, .09f, 1.f); }
float soft10(float x) { return softf(2.f*x, .10f, 1.f); }
float soft11(float x) { return softf(2.f*x, .11f, 1.f); }
float soft12(float x) { return softf(2.f*x, .12f, 1.f); }
float soft13(float x) { return softf(2.f*x, .13f, 1.f); }
float soft14(float x) { return softf(2.f*x, .14f, 1.f); }
float soft15(float x) { return softf(2.f*x, .15f, 1.f); }
float soft16(float x) { return softf(2.f*x, .16f, 1.f); }
float soft17(float x) { return softf(2.f*x, .17f, 1.f); }
float soft18(float x) { return softf(2.f*x, .18f, 1.f); }
float soft19(float x) { return softf(2.f*x, .19f, 1.f); }
float soft20(float x) { return softf(2.f*x, .20f, 1.f); }
float soft21(float x) { return softf(2.f*x, .21f, 1.f); }
float soft22(float x) { return softf(2.f*x, .22f, 1.f); }
float soft23(float x) { return softf(2.f*x, .23f, 1.f); }
float soft24(float x) { return softf(2.f*x, .24f, 1.f); }
float soft25(float x) { return softf(2.f*x, .25f, 1.f); }
float soft26(float x) { return softf(2.f*x, .26f, 1.f); }
float soft27(float x) { return softf(2.f*x, .27f, 1.f); }
float soft28(float x) { return softf(2.f*x, .28f, 1.f); }
float soft29(float x) { return softf(2.f*x, .29f, 1.f); }
float soft30(float x) { return softf(2.f*x, .30f, 1.f); }
float soft31(float x) { return softf(2.f*x, .31f, 1.f); }
float soft32(float x) { return softf(2.f*x, .32f, 1.f); }
float soft33(float x) { return softf(2.f*x, .33f, 1.f); }
float soft34(float x) { return softf(2.f*x, .34f, 1.f); }
float soft35(float x) { return softf(2.f*x, .35f, 1.f); }
float soft36(float x) { return softf(2.f*x, .36f, 1.f); }
float soft37(float x) { return softf(2.f*x, .37f, 1.f); }
float soft38(float x) { return softf(2.f*x, .38f, 1.f); }
float soft39(float x) { return softf(2.f*x, .39f, 1.f); }
float soft40(float x) { return softf(2.f*x, .40f, 1.f); }
float soft41(float x) { return softf(2.f*x, .41f, 1.f); }
float soft42(float x) { return softf(2.f*x, .42f, 1.f); }
float soft43(float x) { return softf(2.f*x, .43f, 1.f); }
float soft44(float x) { return softf(2.f*x, .44f, 1.f); }
float soft45(float x) { return softf(2.f*x, .45f, 1.f); }
float soft46(float x) { return softf(2.f*x, .46f, 1.f); }
float soft47(float x) { return softf(2.f*x, .47f, 1.f); }
float soft48(float x) { return softf(2.f*x, .48f, 1.f); }
float soft49(float x) { return softf(2.f*x, .49f, 1.f); }
float soft50(float x) { return softf(2.f*x, .50f, 1.f); }
float soft51(float x) { return softf(2.f*x, .51f, 1.f); }
float soft52(float x) { return softf(2.f*x, .52f, 1.f); }
float soft53(float x) { return softf(2.f*x, .53f, 1.f); }
float soft54(float x) { return softf(2.f*x, .54f, 1.f); }
float soft55(float x) { return softf(2.f*x, .55f, 1.f); }
float soft56(float x) { return softf(2.f*x, .56f, 1.f); }
float soft57(float x) { return softf(2.f*x, .57f, 1.f); }
float soft58(float x) { return softf(2.f*x, .58f, 1.f); }
float soft59(float x) { return softf(2.f*x, .59f, 1.f); }
float soft60(float x) { return softf(2.f*x, .60f, 1.f); }
float soft61(float x) { return softf(2.f*x, .61f, 1.f); }
float soft62(float x) { return softf(2.f*x, .62f, 1.f); }
float soft63(float x) { return softf(2.f*x, .63f, 1.f); }
float soft64(float x) { return softf(2.f*x, .64f, 1.f); }
float soft65(float x) { return softf(2.f*x, .65f, 1.f); }
float soft66(float x) { return softf(2.f*x, .66f, 1.f); }
float soft67(float x) { return softf(2.f*x, .67f, 1.f); }
float soft68(float x) { return softf(2.f*x, .68f, 1.f); }
float soft69(float x) { return softf(2.f*x, .69f, 1.f); }
float soft70(float x) { return softf(2.f*x, .70f, 1.f); }
float soft71(float x) { return softf(2.f*x, .71f, 1.f); }
float soft72(float x) { return softf(2.f*x, .72f, 1.f); }
float soft73(float x) { return softf(2.f*x, .73f, 1.f); }
float soft74(float x) { return softf(2.f*x, .74f, 1.f); }
float soft75(float x) { return softf(2.f*x, .75f, 1.f); }
float soft76(float x) { return softf(2.f*x, .76f, 1.f); }
float soft77(float x) { return softf(2.f*x, .77f, 1.f); }
float soft78(float x) { return softf(2.f*x, .78f, 1.f); }
float soft79(float x) { return softf(2.f*x, .79f, 1.f); }
float soft80(float x) { return softf(2.f*x, .80f, 1.f); }
float soft81(float x) { return softf(2.f*x, .81f, 1.f); }
float soft82(float x) { return softf(2.f*x, .82f, 1.f); }
float soft83(float x) { return softf(2.f*x, .83f, 1.f); }
float soft84(float x) { return softf(2.f*x, .84f, 1.f); }
float soft85(float x) { return softf(2.f*x, .85f, 1.f); }
float soft86(float x) { return softf(2.f*x, .86f, 1.f); }
float soft87(float x) { return softf(2.f*x, .87f, 1.f); }
float soft88(float x) { return softf(2.f*x, .88f, 1.f); }
float soft89(float x) { return softf(2.f*x, .89f, 1.f); }
float soft90(float x) { return softf(2.f*x, .90f, 1.f); }
float soft91(float x) { return softf(2.f*x, .91f, 1.f); }
float soft92(float x) { return softf(2.f*x, .92f, 1.f); }
float soft93(float x) { return softf(2.f*x, .93f, 1.f); }
float soft94(float x) { return softf(2.f*x, .94f, 1.f); }
float soft95(float x) { return softf(2.f*x, .95f, 1.f); }
float soft96(float x) { return softf(2.f*x, .96f, 1.f); }
float soft97(float x) { return softf(2.f*x, .97f, 1.f); }
float soft98(float x) { return softf(2.f*x, .98f, 1.f); }
float soft99(float x) { return softf(2.f*x, .99f, 1.f); }
float soft00(float x) { return softf(2.f*x, 1.0f, 1.f); }

#if 1
float(*fs[])(float x) = {
    atanf2,
    asinhf2,
    cubicf,
    blunterf,
    tanhf2,
    sinusf,
    erff2,
    sigf,
    logisticf,
    gdf,
    algebraicf,
    transitionf,
    logarithmicf,
};
#else
float(*fs[])(float x) = {
    soft01, soft02, soft03, soft04, soft05, soft06, soft07, soft08, soft09, soft10,
    soft11, soft12, soft13, soft14, soft15, soft16, soft17, soft18, soft19, soft20,
    soft21, soft22, soft23, soft24, soft25, soft26, soft27, soft28, soft29, soft30,
    soft31, soft32, soft33, soft34, soft35, soft36, soft37, soft38, soft39, soft40,
    soft41, soft42, soft43, soft44, soft45, soft46, soft47, soft48, soft49, soft50,
    soft51, soft52, soft53, soft54, soft55, soft56, soft57, soft58, soft59, soft60,
    soft61, soft62, soft63, soft64, soft65, soft66, soft67, soft68, soft69, soft70,
    soft71, soft72, soft73, soft74, soft75, soft76, soft77, soft78, soft79, soft80,
    soft81, soft82, soft83, soft84, soft85, soft86, soft87, soft88, soft89, soft90,
    soft91, soft92, soft93, soft94, soft95, soft96, soft97, soft98, soft99, soft00,
};
#endif

float f_precise_hfc(float(*f)(float x), float out_gain, float in_gain)
{
    fixed_t x[T];
    for (size_t t = 0; t < T; ++t)
        x[t] = A*f(in_gain*sine[t]);

    fixed_t bs[T/4] = {0};
    for (size_t k = 0; k < T/4 - SKIP; ++k) {
        int64_t b = 0;
        for (size_t t = 0; t < T; ++t)
            b += (int64_t)x[t] * sines[k][t];
        bs[k] = b >> FIXED_WIDTH;
    }

    (void)out_gain;
    float sum = 0.f;
    for (size_t i = 1; i < T/4 - SKIP; ++i)
        sum += (2*i+1) * ((float)bs[i]/A) * ((float)bs[i]/A);
    return sqrtf(sum) / ((float)bs[0]/A);
    return sum / (((float)bs[0]/A) * ((float)bs[0]/A));
}

float f_precise_hfc_hardness(float(*f)(float x), float out_gain, float in_gain)
{
    //return f_precise_hfc(f, out_gain, in_gain);

    float hfc_diff_max = 0.f;
    const float da = 1.f/128.f;
    #if 0 // filtering
    const float smoothing = .99f;
    #else
    const float smoothing = 0.f;
    #endif

    float hfc1 = 0.f;
    #if 1 // filtering bias
    float hfc_diff1 = -1.f * f_precise_hfc(f, out_gain, da*in_gain);
    #else
    float hfc_diff1 = 0.f;
    #endif
    for (float a = da; a <= 2.0001f; a += da) {
        float hfc = f_precise_hfc(f, out_gain, a*in_gain);
        float hfc_diff = (1.f - smoothing)*(hfc - hfc1) + smoothing*hfc_diff1;
        if (hfc_diff > hfc_diff_max)
            hfc_diff_max = hfc_diff;
        hfc1 = hfc;
        hfc_diff1 = hfc_diff;
    }
    return hfc_diff_max/da;
}

#ifdef HFC_PRECISE_MAIN
int main(void)
{
    float x[sizeof fs / sizeof fs[0]] = {0};
    float y[sizeof fs / sizeof fs[0]] = {0};

    #pragma omp parallel for
    for (size_t i = 0; i < sizeof fs / sizeof fs[0]; ++i)
    {
        float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
        float* f = f_mem + IIR_TAIL_LENGTH + BASE;
        for (int j = -BASE - IIR_TAIL_LENGTH; j <= BASE + IIR_TAIL_LENGTH; ++j) {
            float x = (float)j / BASE;
            f[j] = fs[i](x);
        }
        float in_gain      = normalized_input_gain(f);
        float out_gain     = normalized_output_gain(f, in_gain);
        float hardness     = f_hardness(f, out_gain, in_gain);
        if (hardness > 256.f) {
            fprintf(stderr, "Warning: skipping %zu: hardness %f too high.\n", i, hardness);
            continue;
        }
        float hfc_hardness = f_precise_hfc_hardness(fs[i], out_gain, in_gain);
        if (hfc_hardness > 100*1000) {
            fprintf(stderr, "Warning: skipping %zu: HFC hardness %f too high.\n", i, hfc_hardness);
            continue;
        }
        x[i] = i;
        x[i] = 1.f/hardness;
        y[i] = 1.f/hardness;
        y[i] = 1.f/hfc_hardness;
    }
    for (size_t i = 0; i < sizeof fs / sizeof fs[0]; ++i)
        printf("%f, %f\n", x[i], y[i]);
}
#endif // HFC_PRECISE_MAIN
