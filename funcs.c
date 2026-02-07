#include "shared.h"

void put_fp(const int f[])
{
    char str[BASE + sizeof""];
    bool value_increasing = true;
    bool diff_decreasing  = true;
    int  diff = f[0];

    for (size_t i = 0; i < BASE; ++i) {
        str[i] = f[i] + '0';
        value_increasing |= f[i] >= f[i-1];
        diff_decreasing  |= f[i] - f[i-1] <= diff;
        diff = f[i] - f[i-1];
    }
    str[BASE] = '\0';

    puts(str);
    assert(value_increasing);
    assert(diff_decreasing);
}

void count(int f[])
{
    f[-2] = f[-1] = 0;
    for (size_t i = 0; i < BASE; ++i)
        f[i] = 1;
    put_fp(f);

    for (size_t i = 0; next_f(&i, f); put_fp(f))
        ;
}

int main(void)
{
    int f[BASE/*negs*/ + 1/*zero*/ + BASE/*poss*/] = {0};
    int*const fp = f + BASE + 1; // positive side of f

    count(fp);
}
