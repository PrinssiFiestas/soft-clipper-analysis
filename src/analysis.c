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

    printf("In gain:  %g\n", in_gain);
    printf("Out gain: %g\n", out_gain);
    printf("Hardness: %g\n", hardness);
}
