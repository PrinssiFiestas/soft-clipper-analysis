#include "shared.h"

void count(int f[])
{
    f[-2] = f[-1] = 0;
    for (size_t i = 0; i < BASE; ++i)
        f[i] = 1;
    put_f(f);

    for (size_t i = 0; next_f(&i, f); put_f(f))
        ;
}

int main(void)
{
    int f[BASE/*negs*/ + 1/*zero*/ + BASE/*poss*/] = {0};
    int*const fp = f + BASE + 1; // positive side of f

    count(fp);
}
