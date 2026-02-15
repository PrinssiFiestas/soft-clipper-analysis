#include "shared.h"

#ifndef COUNT
#define COUNT 0
#endif

int main(void)
{
    int f[1 + BASE] = {0};
    f_init(f);
    #if !COUNT
    f_print(f);
    for (size_t i = 1; f_next(&i, f); f_print(f))
        ;
    #else
    size_t count = 1;
    for (size_t i = 1; f_next(&i, f); ++count)
        ;
    printf("%zu\n", count);
    #endif
}
