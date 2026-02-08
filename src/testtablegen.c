#include "shared.h"
#include <signal.h>

sig_atomic_t g_signum = 0;
void set_signum(int signum)
{
    g_signum = signum;
}

// Calculate next potentially invalid f. Return true if success, false if
// overflow.
bool f_count_next(int f[BASE])
{
    for (size_t i = BASE - 1; i + 1 > 0; --i) {
        f[i]++;
        if (f[i] <= BASE)
            return true;
        f[i] = 1;
    }
    return false;
}

int main(void)
{
    int f_mem[2 * (2/*leading zeros*/ + BASE)] = {0};
    int* f = f_mem + 2;
    int* f_correct = f_mem + 2 + BASE + 2;
    f_set(f_correct, 1);
    f_set(f, 1);
    size_t f_state = 0;

    printf("Testing... ");
    fflush(stdout);

    // The counter goes trough BASE^BASE functions.
    double max_progress = 1;
    for (size_t pow = 0; pow < BASE; ++pow)
        max_progress *= BASE;

    size_t one_percent = max_progress / 100;
    size_t progress = 0;
    size_t progress_counter = 0;
    size_t counter = 0;

    signal(SIGINT,  set_signum);
    signal(SIGQUIT, set_signum);

    while (true)
    {
        bool f_not_done = f_next(&f_state, f);
        do {
            if (g_signum != 0)
                goto end;

            if ( ! f_count_next(f_correct)) {
                if (f_not_done) {
                    fprintf(stderr, "[FAILED]\n");
                    fprintf(stderr, "Expected end of sequence.\n");
                    fprintf(stderr, "Got "); f_print(f);
                    exit(EXIT_FAILURE);
                }
                goto end;
            }
            ++progress_counter;
        } while ( ! f_valid(f_correct));

        if (memcmp(f, f_correct, sizeof(int[BASE])) != 0) {
            fprintf(stderr, "[FAILED]\n");
            fprintf(stderr, "Expected "); f_print(f_correct);
            fprintf(stderr, "Got      "); f_print(f);
            fprintf(stderr, "Counter: %zu\n", counter);
            exit(EXIT_FAILURE);
        }
        ++counter;
        if (progress_counter >= progress*one_percent) {
            ++progress;
            printf("\rTesting... %zu%%", progress);
            fflush(stdout);
        }
    }
    end:;

    printf("\r Testing [PASSED]\n");
    printf("Compared %zu of %zu functions. All matched!\n", counter, progress_counter);
    puts("");
    exit(EXIT_SUCCESS);
}
