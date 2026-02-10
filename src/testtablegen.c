#include "shared.h"
#include <signal.h>

sig_atomic_t g_signum = 0;
void set_signum(int signum)
{
    g_signum = signum;
}

// Calculate next potentially invalid f. Return true if success, false if end of
// sequence.
bool f_count_next(int f[1 + BASE])
{
    for (size_t i = BASE; i > 0; --i) {
        f[i]++;
        if (f[i] <= BASE)
            return true;
        f[i] = 1;
    }
    return false;
}

// Checks if function is increasing and it's derivative is decreasing.
static inline bool f_valid(const int f[1 + BASE])
{
    bool value_increasing = true;
    bool diff_decreasing  = true;
    int  diff = f[1];

    for (size_t i = 1; i < 1 + BASE; ++i) {
        value_increasing = f[i] >= f[i-1];
        diff_decreasing  = f[i] - f[i-1] <= diff;
        if (!value_increasing || !diff_decreasing)
            return false;
        diff = f[i] - f[i-1];
    }

    return true;
}

int main(void)
{
    if (BASE > 14) {
        fprintf(stderr, "BASE %i too high, the test will take forever.\n", BASE);
        exit(EXIT_FAILURE);
    }

    int f[1 + BASE];
    int f_correct[1 + BASE];
    f_set(f_correct, 1);
    f_set(f, 1);
    size_t f_state = 1;

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

    while (g_signum == 0)
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

        if (memcmp(f, f_correct, sizeof(int[1 + BASE])) != 0) {
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
    end:

    printf("\r Testing [PASSED]\n");
    printf("Compared %zu of %zu functions. All matched!\n", counter, progress_counter);
    puts("");
    exit(EXIT_SUCCESS);
}
