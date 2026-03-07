#include "shared.h"
#include <errno.h>

int main(int argc, char* argv[])
{
    const char* result_path = "results/smoothest" BASE_STR ".bin";
    if (argc > 1)
        result_path = argv[1];
    FILE* result_file = fopen(result_path, "rb");
    if (result_file == NULL) {
        fprintf(stderr, "Could not open %s: %s\n", result_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    Work result;
    fread(&result, sizeof result, 1, result_file);
    float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* f = f_mem + IIR_TAIL_LENGTH + BASE;
    f_filter(f, result.f_gen);
    float in_gain  = normalized_input_gain(f);
    float out_gain = normalized_output_gain(f, in_gain);
    float hardness = f_hardness(f, out_gain, in_gain);

    // To be plotted in debugger.
    float f_normalized_mem[BASE + 1 + BASE];
    float* f_normalized = f_normalized_mem + BASE;
    for (int i = -BASE; i <= BASE; ++i) {
        float x = (float)i / BASE;
        f_normalized[i] = out_gain * f_call(f, in_gain*x);
    }
    (void)f_normalized;

    float blunter_mem[BASE + 1 + BASE + 1/*f_hardness() overshoots by one*/];
    float* blunter = blunter_mem + BASE;
    for (int i = -BASE; i <= BASE + 1; ++i) {
        float x = (float)i / BASE;
        blunter[i] = 2.f*x - fabsf(x)*x;
    }
    float blunter_in_gain  = normalized_input_gain(blunter);
    float blunter_out_gain = normalized_output_gain(blunter, blunter_in_gain);
    float blunter_diff[1 + BASE + 1/*plot scale*/];
    float diff_sum = 0.f;
    for (size_t i = 0; i <= BASE; ++i) {
        float x = (float)i / BASE;
        diff_sum += blunter_diff[i] = fabsf(
            blunter_out_gain*f_call(blunter, blunter_in_gain*x) - f_normalized[i]);
    }
    blunter_diff[BASE + 1] = blunter_out_gain;
    if (argc == 1) {
        const char* diff_path = "blunter-smoothest" BASE_STR "-diff.csv";
        FILE* diff_file = fopen(diff_path, "wb");
        if (diff_file != NULL)  {
            for (size_t i = 0; i <= BASE + 1; ++i)
                fprintf(diff_file, "%zu, %f\n", i, blunter_diff[i]);
            printf("Difference of smoothest and Blunter has been written to %s\n", diff_path);
        } else
            fprintf(stderr, "Could not write to %s: %s\n", diff_path, strerror(errno));
    }

    printf("Index:    %zu\n", (size_t)result.f_index);
    printf("In gain:  %g\n", in_gain);
    printf("Out gain: %g\n", out_gain);
    printf("Hardness: %g\n", hardness);
    printf("Softness: %g\n", 1.f/hardness);
    printf("Difference with Blunter: %g (%g%% relative to Blunter's highest value)\n",
           diff_sum / BASE, 100.f * (diff_sum / BASE) / blunter_out_gain);
    #if 0 // for reference
    float blunter_hardness = f_hardness(blunter, blunter_out_gain, blunter_in_gain);
    printf("Blunter hardness: %g\n", blunter_hardness);
    printf("Blunter softness: %g\n", 1.f/blunter_hardness);
    #endif
}
