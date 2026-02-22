// Metaprogram that checks if shader compiles and generates final shader with
// external #defines.

#include "shared.h"
#include <glad/glad.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    Display* display = XOpenDisplay(NULL);
    assert(display != NULL);
    XVisualInfo* visual_info = XGetVisualInfo(display, 0, NULL, &(int){0});
    GLXContext glx_context = glXCreateContext(display, visual_info, NULL, True);
    assert(glx_context != NULL);
    assert(glXIsDirect(display, glx_context) == True);
    Window root = DefaultRootWindow(display);
    int is_current_context = glXMakeCurrent(display, root, glx_context);
    assert(is_current_context == True);

    int gl_version = gladLoadGL();
    assert(gl_version != 0);

    char shader_source[(1 << 14) + sizeof""];
    const char* shader_path = "src/compute.glsl";
    int shader_fd = open(shader_path, O_RDONLY);
    if (shader_fd == -1)
        exit(!!fprintf(stderr, "Could not open %s: %s\n", shader_path, strerror(errno)));
    size_t shader_source_length = read(shader_fd, shader_source, sizeof shader_source);
    if (shader_source_length == (size_t)-1)
        exit(!!fprintf(stderr, "Could not read %s: %s\n", shader_path, strerror(errno)));
    assert(shader_source_length < sizeof shader_source);
    shader_source[shader_source_length] = '\0';
    close(shader_fd);

    char info_log[2048];
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &(const char*){shader_source}, NULL);
    glCompileShader(shader);
    GLint compile_status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == GL_FALSE) {
        glGetShaderInfoLog(shader, sizeof info_log, NULL, info_log);
        fprintf(stderr, "%s\n", info_log);
        exit(EXIT_FAILURE);
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status == GL_FALSE) {
        glGetProgramInfoLog(program, sizeof info_log, NULL, info_log);
        fprintf(stderr, "%s\n", info_log);
        exit(EXIT_FAILURE);
    }

    #if 0
    GLint max_work_group_count[3];
    GLint max_work_group_size[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_work_group_count[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &max_work_group_count[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &max_work_group_count[2]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,  0, &max_work_group_size[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,  1, &max_work_group_size[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,  2, &max_work_group_size[2]);
    #endif

    glXMakeCurrent(display, 0, 0);
    glXDestroyContext(display, glx_context);
    XFree(visual_info);
    XCloseDisplay(display);

    size_t i = 0;
    puts("const char shader_source[] =");
    printf("    \"");
    for (; shader_source[i] != '\n'; ++i) // #version must be first line.
        putchar(shader_source[i]);
    puts("\\n\"");

    printf("    \"#define EXTERNAL_DEFS\\n\"\n");
    printf("    \"#define BASE            %i\\n\"\n", BASE           );
    printf("    \"#define T               %i\\n\"\n", T              );
    printf("    \"#define THD_NORMALIZED  %g\\n\"\n", THD_NORMALIZED );
    printf("    \"#define SKIP            %i\\n\"\n", SKIP           );
    printf("    \"#define IIR_TAIL_LENGTH %i\\n\"\n", IIR_TAIL_LENGTH);
    printf("    \"#define IIR_INTENSITY   %i\\n\"\n", IIR_INTENSITY  );
    printf("    \"#define IIR_POLES       %i\\n\"\n", IIR_POLES      );
    printf("    \"#define MAX_IN_GAIN     %g\\n\"\n", MAX_IN_GAIN    );
    printf("    \"#define CACHE_LINE_SIZE %i\\n\"\n", CACHE_LINE_SIZE);
    printf("    \"#define GPU_WORK_SIZE   %i\\n\"\n", GPU_WORK_SIZE  );
    printf("    \"#define WORK_GROUP_SIZE %i\\n\"\n", WORK_GROUP_SIZE);
    #ifdef GPU_DEBUG
    printf("    \"#define GPU_DEBUG       %i\\n\"\n", GPU_DEBUG      );
    #endif
    puts("    \"\\n\"");

    printf("    \"");
    for (++i; i < shader_source_length; ++i) {
        if (shader_source[i] == '\n') {
            printf("\\n\"\n    \"");
            continue;
        }
        putchar(shader_source[i]);
    }
    puts("\";");
}
