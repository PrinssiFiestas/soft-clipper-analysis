#include "shared.h"
#include "../build/shader_source.c"
#include <glad/glad.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

bool                g_got_gpu;
static Display*     display;
static GLXContext   glx_context;
static XVisualInfo* visual_info;
static GLuint       ssbo;
static GLint        work_length_location;

void GLAPIENTRY
message_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    (void)source; (void)id; (void)length; (void)userParam;

    fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
    fprintf(stderr, "\n\n");
}

bool gpu_init(void)
{
    display = XOpenDisplay(NULL);
    if (display == NULL)
        return !fprintf(stderr, "XOpenDisplay() failed.\n");;
    visual_info = XGetVisualInfo(display, 0, NULL, &(int){0});
    glx_context = glXCreateContext(display, visual_info, NULL, True);
    if (glx_context == NULL || glXIsDirect(display, glx_context) != True)
        return !fprintf(stderr, "Failed creating direct GLX context.\n");;
    Window root = DefaultRootWindow(display);
    int is_current_context = glXMakeCurrent(display, root, glx_context);
    if (is_current_context != True)
        return !fprintf(stderr, "Failed making GLX context the current context.\n");

    int gl_version = gladLoadGL();
    if (gl_version == 0)
        return !fprintf(stderr, "gladLoadGL() failed.\n");;

    char info_log[2048];
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &(const char*){shader_source}, NULL);
    glCompileShader(shader);
    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == GL_FALSE) {
        glGetShaderInfoLog(shader, sizeof info_log, NULL, info_log);
        return !fprintf(stderr, "%s\n", info_log);
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        glGetProgramInfoLog(program, sizeof info_log, NULL, info_log);
        return !fprintf(stderr, "%s\n", info_log);
    }
    work_length_location = glGetUniformLocation(program, "g_work_length");
    glUseProgram(program);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(message_callback, 0);
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

    return g_got_gpu = true;
}

void gpu_compute(size_t buffer_length, size_t buffer_element_size, void* buffer)
{
    glUniform1ui(work_length_location, buffer_length);

    size_t work_size = buffer_element_size * buffer_length;

    glBufferData(GL_SHADER_STORAGE_BUFFER, work_size, buffer, GL_STATIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glDispatchCompute((buffer_length + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE, 1, 1);
    glFlush();
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, work_size, buffer);
    glDeleteSync(sync);
}

#if BENCH
static uint64_t t_gpu_cpu_work; // distributing work and collecting results.
static uint64_t t_gpu_gpu_work; // actual work done on GPU
#endif

Work gpu_do_work(const Work* in_work)
{
    static Work work_units[WORK_SIZE / GPU_WORK_SIZE];

    #if BENCH
    __uint128_t t_start = time_begin();
    __asm__ __volatile__("":::"memory");
    #endif

    size_t result_index = 0;
    size_t work_length = 0;
    #ifndef GPU_MAIN
    size_t tries = 0;
    try_work:;
    if (tries++ > 2) {
        fprintf(stderr, "GPU failed work at index %zu (global %llu) of %zu jobs.\n",
                result_index, (unsigned long long)in_work->f_index, work_length);
        fprintf(stderr, "Handing off work to CPU...\n\n\n");
        extern void cpu_do_work(Work* result);
        Work work = *in_work;
        cpu_do_work(&work);
        return work;
    }
    #endif
    Work work = *in_work;
    work.f_hardness = 0.f;
    work_length = 0;

    do {
        if ((work.f_index & (GPU_WORK_SIZE - 1)) == 0)
            work_units[work_length++] = work;
        work.f_index++;
    } while (work_length < WORK_SIZE / GPU_WORK_SIZE && f_next(&work.f_state, work.f_gen));

    #if BENCH
    __uint128_t t_gpu_start = time_begin();
    __asm__ __volatile__("":::"memory");
    #endif
    gpu_compute(work_length, sizeof work, work_units);
    #if BENCH
    __asm__ __volatile__("":::"memory");
    uint64_t t_gpu = time_begin() - t_gpu_start;
    t_gpu_gpu_work += t_gpu;
    #endif

    result_index = 0;
    for (size_t i = 0; i < work_length; ++i) {
        #ifndef GPU_MAIN
        if (work_units[i].f_hardness == 0.f) // shader interrupted
            goto try_work;
        #endif
        if (work_units[i].f_hardness < work_units[result_index].f_hardness)
            result_index = i;
    }

    #if BENCH
    __asm__ __volatile__("":::"memory");
    t_gpu_cpu_work += (time_begin() - t_start) - t_gpu;
    #endif
    return work_units[result_index];
}

void gpu_destroy(void)
{
    if ( ! g_got_gpu) // may leak, I don't care.
        return;
    glXMakeCurrent(display, 0, 0);
    glXDestroyContext(display, glx_context);
    XFree(visual_info);
    XCloseDisplay(display);
}

#if BENCH
#define WORK_INTERVAL WORK_SIZE
#else
#define WORK_INTERVAL 53 // somewhat random on purpose
#endif

#ifdef GPU_MAIN
int main(void)
{
    bool got_gpu = gpu_init();
    assert(got_gpu);

    static Work work_gpu[1 << 16];
    static Work work_cpu[1 << 16];
    size_t work_length = 0;

    puts("Generating work...");

    Work w = {.f_state = 1 };
    f_init(w.f_gen);
    do { // generate work
        if ((w.f_index % WORK_INTERVAL) == 0) {
            work_cpu[work_length] = work_gpu[work_length] = w;
            work_length++;
        }
        w.f_index++;
    } while (work_length < sizeof work_gpu/sizeof work_gpu[0]
                && f_next(&w.f_state, w.f_gen));

    puts("Done generating work. Working on GPU...");

    uint64_t work_unit_time = 0; (void)work_unit_time;
    __uint128_t gpu_time_start = time_begin();
    __asm__ __volatile__("":::"memory");
    #if BENCH
    for (size_t i = 0; i < work_length; ++i) {
        __uint128_t t_start = time_begin();
        __asm__ __volatile__("":::"memory");
        work_gpu[i] = gpu_do_work(&work_gpu[i]);
        __asm__ __volatile__("":::"memory");
        work_unit_time += time_begin() - t_start;
    }
    #else
    gpu_compute(work_length, sizeof w, work_gpu);
    #endif
    __asm__ __volatile__("":::"memory");
    double gpu_time = time_diff(gpu_time_start);
    gpu_destroy();

    #if BENCH
    printf("Done working on GPU. Average work unit time: %g\n",
           .000000001*work_unit_time/work_length);
    printf("Collecting GPU results was %g%% of total GPU work\n",
           100.*t_gpu_cpu_work/(t_gpu_gpu_work + t_gpu_cpu_work));
    puts("Working on CPU...");
    #else
    puts("Done working on GPU. Working on CPU...");
    #endif

    __uint128_t cpu_time_start = time_begin();
    __asm__ __volatile__("":::"memory");
    #if BENCH
    for (size_t i = 0; i < work_length; ++i) {
        Work work = work_cpu[i];
        work_cpu[i].f_hardness = 1e10f;
        do {
            float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
            float* f = f_mem + IIR_TAIL_LENGTH + BASE;
            f_filter(f, work.f_gen);
            float in_gain  = normalized_input_gain(f);
            if (in_gain > MAX_IN_GAIN)
                continue;
            float out_gain = normalized_output_gain(f, in_gain);
            work.f_hardness = f_hardness(f, out_gain, in_gain);
            if (work.f_hardness < work_cpu[i].f_hardness) {
                work_cpu[i] = work;
                #if GPU_DEBUG
                work_cpu[i].value1 = in_gain;
                work_cpu[i].value2 = out_gain;
                #endif
            }
        } while ((++work.f_index & (WORK_SIZE-1)) && f_next(&work.f_state, work.f_gen));
    }
    #else // only need to work once
    for (size_t i = 0; i < work_length; ++i) {
        float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
        float* f = f_mem + IIR_TAIL_LENGTH + BASE;
        f_filter(f, work_cpu[i].f_gen);
        float in_gain  = normalized_input_gain(f);
        float out_gain = normalized_output_gain(f, in_gain);
        work_cpu[i].f_hardness = f_hardness(f, out_gain, in_gain);
        #if GPU_DEBUG
        work_cpu[i].value1 = in_gain;
        work_cpu[i].value2 = out_gain;
        #endif
    }
    #endif
    __asm__ __volatile__("":::"memory");
    double cpu_time = time_diff(cpu_time_start);

    float ratios_mean = 0.f;
    for (size_t i = 0; i < work_length; ++i)
        if (work_gpu[i].f_hardness < 1e10f && work_cpu[i].f_hardness < 1e10f)
            #if 0 //GPU_DEBUG
            ratios_mean += work_gpu[i].value1 / work_cpu[i].value1
                + work_gpu[i].value2 / work_cpu[i].value2
                ;
            #else
            ratios_mean += work_gpu[i].f_hardness / work_cpu[i].f_hardness;
            #endif
        // else ignore discarded values.
    ratios_mean /= work_length;

    bool failed = false;
    printf("Total work count: %zu\n", w.f_index);
    printf("Work length:      %zu\n", work_length);
    #if BENCH
    #else
    if (fabsf(1.f - ratios_mean) > .02f)
        failed = fprintf(stderr, "[FAILED] ratios mean test! Expected close to zero difference.\n");
    else
        printf("[PASSED] ratios mean test.\n");
    #endif

    printf("GPU and CPU implementations have %g%% difference.\n", 100.f*fabsf(1.f - ratios_mean));
    if ( ! failed) {
        printf("CPU time: %g\n", cpu_time);
        printf("GPU time: %g\n", gpu_time);
        printf("Time ratio: %g%%\n", 100.*cpu_time/gpu_time);
    }
    exit(failed);
}
#endif // GPU_MAIN
