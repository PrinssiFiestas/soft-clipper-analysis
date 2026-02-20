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

#if 0 // TODO move this
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

    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
        return;
    fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}
#endif

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
    //glEnable(GL_DEBUG_OUTPUT); // TODO we don't need these here, but later do!
    //glDebugMessageCallback(message_callback, 0);

    #if EXECUTE_SHADER // TODO we don't execute here, move to appropriate file.
    int data = -1;
    GLuint ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof data, &data, GL_STATIC_READ);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    #endif

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

    char info_log[256];
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

    #if EXECUTE_SHADER // TODO we don't execute here, move to appropriate file.
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof data, &data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    #endif

    glXMakeCurrent(display, 0, 0);
    glXDestroyContext(display, glx_context);
    XFree(visual_info);
    XCloseDisplay(display);

    puts("const char shader_source[] ="); // TODO `#version 430 core` has to be first line!
    printf("    \"");
    for (size_t i = 0; i < shader_source_length; ++i) {
        if (shader_source[i] == '\n') {
            printf("\\n\"\n    \"");
            continue;
        }
        putchar(shader_source[i]);
    }
    puts("\";");
}
