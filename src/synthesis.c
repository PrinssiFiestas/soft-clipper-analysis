// Metaprogram to precalculate sinusoids.

#include "shared.h"
#include <math.h>

// Synthesis mode
// #define CSV
#define C_HEADER

int main(void)
{
    // First half shall be the negative side, next half for positives.
    #ifdef CSV
    for (int i = 0; i < T; ++i) {
        int sine = .5 - A*sin(2*M_PI*(i+.5) / T);
        printf("%i, %i\n", i - BASE, sine);
    }
    #endif

    #ifdef C_HEADER
    printf("// Float sine with frequency of 1/T.\n");
    printf("const float sine[%i] = {", T);
    for (int t = 0; t < T; ++t) {
        if ((t & (4-1)) == 0)
            printf("\n    ");
        double sine = -sin(2*M_PI*(t+.5) / T);
        printf("%af, ", sine);
    }
    puts("\n};\n");

    printf("// Fixed width sines with frequencies of multiples of 1/T.\n");
    printf("const int sines[%i][%i] = {\n", T/2, T);
    for (int k = 0; k < T/2; ++k) {
        printf("    [ %i ] = {", k);
        for (int t = 0; t < T; ++t) {
            if ((t & 7) == 0)
                printf("\n        ");
            int sine = .5 - A*sin(2*M_PI*k*(t+.5) / T);
            printf("%i, ", sine);
        }
        printf("\n    },\n");
    }
    puts("};");
    #endif
}
