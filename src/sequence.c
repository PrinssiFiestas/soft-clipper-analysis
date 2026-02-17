#include "shared.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

_Atomic uint64_t g_sequence_length = 0;

void* estimate_sequence_length(void*_)
{
    (void)_;
    #define CACHE_STR "Completed: "

    char cache_path[64] = "";
    if (access("build", F_OK) == 0)
        strcat(cache_path, "build/");
    strcat(cache_path, "seqcount" BASE_STR ".cache");

    int f_fallback_buf[1 + BASE] = {0}; // in case of failing to cache.
    int* f = f_fallback_buf;
    size_t* f_state = &(size_t){1};
    uint64_t* count = &(uint64_t){1};
    f_init(f);
    bool cache_found = access(cache_path, F_OK) == 0;
    void* cache_contents = NULL;
    int cache_fd = open(cache_path, O_RDWR | O_CREAT, 0666);
    ftruncate(cache_fd, 4096);

    if (cache_fd == -1) {
        fprintf(stderr, "Could not open %s: %s\n", cache_path, strerror(errno));
        puts("Continuing sequence length estimation without caching...");
    } else {
        cache_contents = mmap(
            NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, cache_fd, 0);
        if ((intptr_t)cache_contents <= 0) {
            fprintf(stderr, "Could not map %s to memory: %s\n", cache_path, strerror(errno));
            puts("Continuing sequence length estimation without caching...");
        } else if (strncmp(cache_contents, CACHE_STR, sizeof CACHE_STR - 1) == 0) {
            printf("Found completed sequence length cache. Delete %s for timing.\n", cache_path);
            printf("Sequence length: %s\n", (char*)cache_contents + sizeof CACHE_STR - 1);
            exit(EXIT_SUCCESS);
        } else if (cache_found) {
            fprintf(stderr,
                "[WARNING] Incomplete cache may be corrupted. Results may not be accurate.\n");
            printf("Found incomplete sequence length estimation cache. Continuing estimation...\n");
            count = cache_contents;
            f_state = (size_t*)(count + 1);
            f = (int*)(f_state + 1);
        } else {
            puts("No sequence length estimation cache found.");
            puts("Starting estimation from the beginning...");
        }
    }

    __uint128_t t_start = time_begin();
    __asm__ __volatile__("":::"memory");
    for (; f_next(f_state, f); ++*count)
        ;
    __asm__ __volatile__("":::"memory");
    double t = time_diff(t_start);
    printf("Took %g seconds to count sequence length of base %i.\n", t, BASE);
    printf("Sequence length: %llu\n", (unsigned long long)*count);
    g_sequence_length = *count;

    if ((intptr_t)cache_contents > 0) {
        size_t contents_length = sprintf(cache_contents, CACHE_STR "%llu\n", (unsigned long long)*count);
        msync(cache_contents, contents_length, MS_SYNC);
        ftruncate(cache_fd, contents_length);
    }
    return NULL;
}

#ifdef SEQUENCE_MAIN
int main(void)
{
    #if !COUNT
    int f[1 + BASE] = {0};
    f_init(f);
    f_print(f);
    for (size_t i = 1; f_next(&i, f); f_print(f))
        ;
    #else
    estimate_sequence_length(NULL);
    #endif
}
#endif // SEQUENCE_MAIN
