#include "glext.h"

#include <SDL2/SDL.h>

void (*glGenBuffers)(GLsizei, GLuint*);
void (*glBindBuffer)(GLenum, GLuint);
void (*glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
void (*glDeleteBuffers)(GLsizei, const GLuint*);

GLuint (*glCreateShader)(GLenum);
void (*glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
void (*glCompileShader)(GLuint);
void (*glGetShaderiv)(GLuint, GLenum, GLint*);
void (*glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
void (*glDeleteShader)(GLuint);

GLuint (*glCreateProgram)();
void (*glAttachShader)(GLuint, GLuint);
void (*glLinkProgram)(GLuint);
void (*glGetProgramiv)(GLuint, GLenum, GLint*);
void (*glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
void (*glUseProgram)(GLuint);
void (*glDeleteProgram)(GLuint);

GLint (*glGetAttribLocation)(GLuint, const GLchar*);
GLint (*glGetUniformLocation)(GLuint, const GLchar*);
void (*glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
void (*glUniform1i)(GLint, GLint);

void (*glEnableVertexAttribArray)(GLuint);
void (*glDisableVertexAttribArray)(GLuint);
void (*glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

namespace {
template <typename T>
bool load(T& fn, const char* name) {
    fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    return fn != nullptr;
}
} // namespace

bool loadGLExtensions() {
    bool ok = true;
    ok &= load(glGenBuffers, "glGenBuffers");
    ok &= load(glBindBuffer, "glBindBuffer");
    ok &= load(glBufferData, "glBufferData");
    ok &= load(glDeleteBuffers, "glDeleteBuffers");

    ok &= load(glCreateShader, "glCreateShader");
    ok &= load(glShaderSource, "glShaderSource");
    ok &= load(glCompileShader, "glCompileShader");
    ok &= load(glGetShaderiv, "glGetShaderiv");
    ok &= load(glGetShaderInfoLog, "glGetShaderInfoLog");
    ok &= load(glDeleteShader, "glDeleteShader");

    ok &= load(glCreateProgram, "glCreateProgram");
    ok &= load(glAttachShader, "glAttachShader");
    ok &= load(glLinkProgram, "glLinkProgram");
    ok &= load(glGetProgramiv, "glGetProgramiv");
    ok &= load(glGetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load(glUseProgram, "glUseProgram");
    ok &= load(glDeleteProgram, "glDeleteProgram");

    ok &= load(glGetAttribLocation, "glGetAttribLocation");
    ok &= load(glGetUniformLocation, "glGetUniformLocation");
    ok &= load(glUniformMatrix4fv, "glUniformMatrix4fv");
    ok &= load(glUniform1i, "glUniform1i");

    ok &= load(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= load(glDisableVertexAttribArray, "glDisableVertexAttribArray");
    ok &= load(glVertexAttribPointer, "glVertexAttribPointer");
    return ok;
}
