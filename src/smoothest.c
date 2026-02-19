#include "shared.h"
#include <stdatomic.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#ifndef NPROC
#define NPROC 8
#endif

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

typedef struct work
{
    _Alignas(CACHE_LINE_SIZE)
    uint64_t f_index;
    uint32_t f_state;
    float    f_hardness;
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
Work   g_result;

#define WORK_SIZE (1lu << 12)
_Static_assert((WORK_SIZE & (WORK_SIZE - 1)) == 0, "WORK_SIZE must be a power of two.");

// Total work amount.
extern _Atomic bool g_got_sequence_length;
extern uint64_t g_sequence_length;
extern void* estimate_sequence_length(void* unused);
__uint128_t g_total_time_start;

static void* do_work(void* worker);
static void* collect_results(void* result);
static __uint128_t dispatch_work(uint64_t f_index, uint32_t f_state, int f_gen[1 + BASE]);

int main(void)
{
    g_result.f_hardness = 1e10f;
    g_total_time_start = time_begin();
    char time_buf[70];
    if (get_date(time_buf) != NULL)
        printf("Starting work on %s\n", time_buf);

    char backup_path[64] = "";
    if (access("build", F_OK) == 0)
        strcat(backup_path, "build/");
    strcat(backup_path, "backup" BASE_STR ".cache");
    bool backup_found = access(backup_path, F_OK) == 0;
    int backup_fd = open(backup_path, O_RDWR | O_CREAT, 0666);
    Work backup = {0};
    if (backup_found && backup_fd != -1) {
        if (read(backup_fd, &backup, sizeof backup) != -1) {
            g_result = backup;
            puts("Found backup. Continuing where left off...");
        } else {
            fprintf(stderr, "Could not read from %s: %s\n", backup_path, strerror(errno));
            puts("Backup not read, starting from beginning...");
        }
    }
    if (backup_fd == -1) {
        if (backup_found)
            fprintf(stderr, "Could not open %s: %s\n", backup_path, strerror(errno));
        else
            fprintf(stderr, "Could not create %s: %s\n", backup_path, strerror(errno));
        puts("Continuing without backing up...");
    }

    pthread_t thrds[NPROC + 2];
    pthread_create(&thrds[NPROC + 0], NULL, estimate_sequence_length, NULL);
    pthread_create(&thrds[NPROC + 1], NULL, collect_results, (void*)(intptr_t)backup_fd);
    for (size_t i = 0; i < NPROC; ++i)
        pthread_create(&thrds[i], NULL, do_work, (void*)i);

    int f_gen[1 + BASE];
    f_init(f_gen);
    uint32_t f_state = 1;
    uint64_t f_index = 0;
    if (backup_found && backup_fd != -1) {
        memcpy(f_gen, backup.f_gen, sizeof f_gen);
        f_state = backup.f_state;
        f_index = backup.f_index;
    }
    __uint128_t dispatch_sleep_time = 0;
    do { // generate work
        if ((f_index & (WORK_SIZE - 1)) == 0)
            dispatch_sleep_time += dispatch_work(f_index, f_state, f_gen);
    } while (++f_index, f_next(&f_state, f_gen));

    g_work_done = true;
    for (size_t i = 0; i < sizeof thrds / sizeof thrds[0]; ++i)
        pthread_join(thrds[i], NULL);

    double total_time = time_diff(g_total_time_start);
    if (get_date(time_buf) != NULL)
        printf("Finished work on %s\n", time_buf);
    if (time_str(time_buf, total_time) != NULL)
        printf("Total time taken%s: %s\n",
               total_time < 60*60*24*30 ? "" : "(roughly)",
               time_buf);
    printf("Dispatcher was asleep %g%% of total time.\n\n",
           100. * ((double)dispatch_sleep_time/1000000000.) / total_time);

    printf("Found smoothest function.\n");
    printf("Smoothest function index: %llu\n", (unsigned long long)g_result.f_index);
    printf("Smoothest function hardness: %g\n", g_result.f_hardness);

    float f_result_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
    float* f_result = f_result_mem + IIR_TAIL_LENGTH + BASE;
    f_filter(f_result, g_result.f_gen);
    float f_in_gain = normalized_input_gain(f_result);
    float f_out_gain = normalized_output_gain(f_result, f_in_gain);

    const char* result_csv_path = "smoothest" BASE_STR ".csv";
    FILE* result_csv_file;
    while ((result_csv_file = fopen(result_csv_path, "wb")) == NULL) {
        fprintf(stderr, "Failed opening result file %s: %s\n", result_csv_path, strerror(errno));
        printf("Trying to open %s in a second...\n", result_csv_path);
        usleep(1000*1000);
    }
    bool no_error = true;
    for (size_t i = 0; no_error && i <= BASE; ++i) {
        float x = (float)i/BASE;
        float y = f_out_gain * f_call(f_result, f_in_gain*x);
        no_error = fprintf(result_csv_file, "%g, %g\n", x, y) > 0;
    }
    if (no_error)
        printf("Result data succesfully written to %s\n", result_csv_path);
    else
        fprintf(stderr, "Failed writing result to %s: %s\n", result_csv_path, strerror(errno));

    const char* result_bin_path = "smoothest" BASE_STR ".bin";
    FILE* result_bin_file;
    while ((result_bin_file = fopen(result_bin_path, "wb")) == NULL) {
        fprintf(stderr, "Failed opening binary result file %s: %s\n", result_bin_path, strerror(errno));
        printf("Trying to open %s in a second...\n", result_bin_path);
        usleep(1000*1000);
    }
    if (fwrite(&g_result, sizeof g_result, 1, result_bin_file) == 1)
        printf("Result binary data succesfully written to %s\n", result_bin_path);
    else
        fprintf(stderr, "Failed writing result binary to %s: %s\n", result_bin_path, strerror(errno));
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
    if ( ! g_work_done)
        goto try_dispatch;
    return sleep_time;
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
                usleep(100);

        Work* result = &g_work[(uintptr_t)worker_id];
        result->f_hardness = 1e10f;
        Work work = *result;

        do { // work
            float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
            float* f = f_mem + IIR_TAIL_LENGTH + BASE;
            f_filter(f, work.f_gen);
            float in_gain  = normalized_input_gain(f);
            if (in_gain > MAX_IN_GAIN)
                continue;
            float out_gain = normalized_output_gain(f, in_gain);
            work.f_hardness = f_hardness(f, out_gain, in_gain);
            if (work.f_hardness < result->f_hardness)
                *result = work;
        } while ((++work.f_index & (WORK_SIZE-1)) && f_next(&work.f_state, work.f_gen));

        *me = GOT_RESULT;
    }

    return NULL;
}

static void report_time_estimate(uint64_t progress_counter, uint64_t init_progress)
{
    double time;
    static double t;
    static bool initialized;
    static __uint128_t last_call;
    if (last_call > 0 && time_diff(last_call) < .1) // avoid repeated printing
        return;
    if ((time = time_diff(g_total_time_start)) < 1.) // give time for initial messages
        return;

    if ( ! g_got_sequence_length) // paranoid double check
        return;
    if ( ! initialized)
        initialized = printf("\n\n");

    char animation[4][4] = {"   ", ".  ", ".. ", "..."};
    char buf[TIME_STR_BUF_SIZE];
    double progress = (double)progress_counter/(g_sequence_length - init_progress);
    double smoothing = t == 0. ? -1. : .9; // bias up at beginning
    if (progress > 0)
        t = (1. - smoothing) * (time/progress - time) + smoothing * t;

    printf("\e[2A"); // cursor up
    printf("                                                                         \r");
    printf("Estimated time remaining: %s\n", time_str(buf, t));
    printf("                                                                         \r");
    printf("Working%s %.2f%%\n", animation[(int)(3.*time) & 3], 100.*progress);

    last_call = time_begin();
}

static void* collect_results(void*_backup_fd)
{
    uint64_t init_progress = g_result.f_index;
    uint64_t progress = 0;
    int backup_fd = (intptr_t)_backup_fd;

    try_collect_result:
    for (size_t i = 0; i < NPROC; ++i) {
        if (g_workers[i] == GOT_RESULT) {
            if (g_work[i].f_hardness < g_result.f_hardness)
                g_result = g_work[i];
            g_workers[i] = WAITING;
            progress += WORK_SIZE;
        }
    }
    if (g_got_sequence_length)
        report_time_estimate(progress, init_progress);

    if (backup_fd != -1) {
        if (lseek(backup_fd, 0, SEEK_SET) == -1 ||
            write(backup_fd, &g_result, sizeof g_result) == -1)
        {
            fprintf(stderr, "Could not write to backup file: %s\n", strerror(errno));
            puts("Continuing without backup...");
            close(backup_fd);
            backup_fd = -1;
        }
    }

    usleep(100);
    if ( ! g_work_done)
        goto try_collect_result;

    for (size_t i = 0; i < NPROC; ++i) // collect final work.
        if (g_workers[i] != WAITING)
            goto try_collect_result;
    return NULL;
}
