#include "shared.h"

void count(int f[])
{
    f[-2] = f[-1] = 0;
    for (size_t i = 0; i < BASE; ++i)
        f[i] = 1;
    f_print(f);

    for (size_t i = 0; f_next(&i, f); f_print(f))
        ;
}

int main(void)
{
    int f[1 + BASE] = {0};
    f_init(f);
    f_print(f);
    for (size_t i = 1; f_next(&i, f); f_print(f))
        ;
}
