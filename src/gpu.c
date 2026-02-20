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

    //if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    //    return;
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
    glUseProgram(program);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(message_callback, 0);
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

    return g_got_gpu = true;
}

void gpu_compute(size_t buffer_length, size_t buffer_element_size, void* buffer)
{
    size_t work_size = buffer_element_size * buffer_length;
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo); // TODO is this necessary?
    glBufferData(GL_SHADER_STORAGE_BUFFER, work_size, buffer, GL_STATIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glDispatchCompute(buffer_length, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, work_size, buffer);
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // TODO is this necessary?
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

#include "shared.h"
#define WORK_INTERVAL 53 // somewhat random on purpose

int main(void)
{
    bool got_gpu = gpu_init();
    assert(got_gpu);

    Work work_gpu[128];
    Work work_cpu[128];
    size_t work_length = 0;

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

    gpu_compute(work_length, sizeof w, work_gpu);
    gpu_destroy();

    for (size_t i = 0; i < work_length; ++i) {
        float f_mem[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];
        float* f = f_mem + IIR_TAIL_LENGTH + BASE;
        f_filter(f, work_cpu[i].f_gen);
        float in_gain  = normalized_input_gain(f);
        float out_gain = normalized_output_gain(f, in_gain);
        work_cpu[i].f_hardness = f_hardness(f, out_gain, in_gain);
        #if GPU_DEBUG
        work_cpu[i].in_gain  = in_gain;
        work_cpu[i].out_gain = out_gain;
        #endif
    }

    float ratios_mean = 0.f;
    for (size_t i = 0; i < work_length; ++i)
        ratios_mean += work_gpu[i].f_hardness / work_cpu[i].f_hardness;
    ratios_mean /= work_length;

    bool failed = false;
    if (fabsf(1.f - ratios_mean) > .02f)
        failed = fprintf(stderr, "[FAILED] ratios mean test! Expected close to zero difference.\n");
    else
        printf("[PASSED] ratios mean test.");
    printf("GPU and CPU implementations have %g%% difference.\n", 100.f*fabsf(1.f - ratios_mean));
    exit(failed);
}
