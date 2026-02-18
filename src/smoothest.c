#include "shared.h"
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>

// TODO probably remove lfc.h

#ifndef NPROC // TODO put in Makefile
#define NPROC 8
#endif

#ifndef CACHE_LINE_SIZE // TODO check from Makefile?
#define CACHE_LINE_SIZE 64
#endif

typedef struct work
{
    _Alignas(CACHE_LINE_SIZE)
    uint64_t f_index;
    uint32_t f_state;
    float    f_hardness; // result of work
    int      f_gen[1 + BASE];
} Work;

typedef _Atomic enum worker_state
{
    WAITING,
    BUSY,
    GOT_RESULT,
} Worker;

_Atomic bool g_work_done = false;
Work   g_work[NPROC];
Worker g_workers[NPROC];

#define WORK_SIZE (1lu << 12) // must be power of 2

// Total work amount.
extern _Atomic uint64_t g_sequence_length;
extern void* estimate_sequence_length(void* unused);

static void* do_work(void* worker);
static void* collect_results(void* unused);
static __uint128_t dispatch_work(uint64_t f_index, uint32_t f_state, int f_gen[1 + BASE]);

int main(void)
{
    __uint128_t total_time_start = time_begin();
    char time_buf[70];
    if (get_date(time_buf) != NULL)
        printf("Starting work on %s\n", time_buf);

    pthread_t thrds[NPROC + 2];
    pthread_create(&thrds[NPROC + 0], NULL, estimate_sequence_length, NULL);
    pthread_create(&thrds[NPROC + 1], NULL, collect_results, NULL);
    for (size_t i = 0; i < NPROC; ++i)
        pthread_create(&thrds[i], NULL, do_work, (void*)i);

    int f_gen[1 + BASE];
    f_init(f_gen);
    uint32_t f_state = 1;
    uint64_t f_index = 0;
    __uint128_t dispatch_sleep_time = 0;
    do { // generate work
        if ((f_index & (WORK_SIZE - 1)) == 0)
            dispatch_sleep_time += dispatch_work(f_index, f_state, f_gen);
    } while (f_next(&f_state, f_gen));

    g_work_done = true;
    for (size_t i = 0; i < sizeof thrds / sizeof thrds[0]; ++i)
        pthread_join(thrds[i], NULL);

    double total_time = time_diff(total_time_start);
    if (get_date(time_buf) != NULL)
        printf("Finished work on %s\n", time_buf);
    if (time_str(time_buf, total_time) != NULL)
        printf("Total time taken%s: %s\n",
               total_time < 60*60*24*30 ? "" : "(roughly)",
               time_buf);
    printf("Dispatcher was asleep %g%% of total time.\n",
           100. * ((double)dispatch_sleep_time/1000000000.) / total_time);
}

static __uint128_t dispatch_work(
    uint64_t f_index, uint32_t f_state, int f_gen[restrict 1 + BASE])
{
    __uint128_t sleep_time = 0;

    try_dispatch:
    for (size_t i = 0; i < NPROC; ++i) {
        if (g_workers[i] == WAITING) {
            g_work[i].f_index = f_index;
            g_work[i].f_state = f_state;
            memcpy(g_work[i].f_gen, f_gen, sizeof g_work[i].f_gen);
            g_workers[i] = BUSY;
            return sleep_time;
        }
    }
    __uint128_t sleep_start = time_begin();
    usleep(1000);
    sleep_time += time_begin() - sleep_start;
    goto try_dispatch;
}

static void* do_work(void* worker_id)
{
    Worker* me = &g_workers[(uintptr_t)worker_id];

    while ( ! g_work_done)
    {
        while (*me != BUSY)
            if (g_work_done)
                return NULL;
            else
                usleep(1000);

        size_t  count  = 0;
        Work*   work   = &g_work[(uintptr_t)worker_id];
        do { // work
            // TODO do work
        } while (count++ < WORK_SIZE && f_next(&work->f_state, work->f_gen));

        *me = GOT_RESULT;
    } // while ( ! g_work_done)

    return NULL;
}

static void* collect_results(void*_) // TODO report time estimates
{
    try_collect_result:
    for (size_t i = 0; i < NPROC; ++i) {
        if (g_workers[i] == GOT_RESULT) {
            // TODO collect results
            g_workers[i] = WAITING;
        }
    }

    usleep(1000);
    if ( ! g_work_done)
        goto try_collect_result;

    for (size_t i = 0; i < NPROC; ++i) // collect final work.
        if (g_workers[i] != WAITING)
            goto try_collect_result;
    return _;
}
