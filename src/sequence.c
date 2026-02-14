#include "shared.h"

int main(void)
{
    int f[1 + BASE] = {0};
    f_init(f);
    f_print(f);
    for (size_t i = 1; f_next(&i, f); f_print(f))
        ;
}
