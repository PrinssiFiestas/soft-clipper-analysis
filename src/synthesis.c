// Metaprogram to precalculate sinusoids of odd harmonics.

#include "shared.h"

int main(void)
{
    puts("#include \"shared.h\"\n");

    printf("// Sine with frequency of 1/T.\n");
    printf("const float sine[%i] = {", T);
    for (int t = 0; t < T; ++t) {
        if ((t & (4-1)) == 0)
            printf("\n    ");
        double sine = -sin(2*M_PI*(t+.5) / T);
        printf("%af, ", sine);
    }
    puts("\n};\n");

    printf("// Fixed point sines with frequencies of odd multiples of 1/T.\n");
    printf("fixed_t sines[%i][%i] = {\n", T/4, T);

    for (int i = 0; i < T/4; ++i) {
        printf("    [ %i ] = {", i);
        for (int t = 0; t < T; ++t) {
            if ((t & 7) == 0)
                printf("\n        ");
            int sine = .5 - A*sin(2*M_PI*(2*i+1)*(t+.5) / T);
            printf("%i, ", sine);
        }
        printf("\n    },\n");
    }
    puts("};");
}
