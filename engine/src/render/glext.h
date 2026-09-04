#pragma once

// Minimal manual loader for the GL 2.0 (shader/VBO) functions we need.
// <GL/gl.h> on Linux only declares GL 1.1, so anything past that has to be
// fetched via SDL_GL_GetProcAddress — this avoids pulling in GLEW/GLAD for
// the ~20 functions we actually use.

#include <GL/gl.h>
#include <cstddef>

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;

extern void (*glGenBuffers)(GLsizei, GLuint*);
extern void (*glBindBuffer)(GLenum, GLuint);
extern void (*glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void (*glDeleteBuffers)(GLsizei, const GLuint*);

extern GLuint (*glCreateShader)(GLenum);
extern void (*glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern void (*glCompileShader)(GLuint);
extern void (*glGetShaderiv)(GLuint, GLenum, GLint*);
extern void (*glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*glDeleteShader)(GLuint);

extern GLuint (*glCreateProgram)();
extern void (*glAttachShader)(GLuint, GLuint);
extern void (*glLinkProgram)(GLuint);
extern void (*glGetProgramiv)(GLuint, GLenum, GLint*);
extern void (*glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*glUseProgram)(GLuint);
extern void (*glDeleteProgram)(GLuint);

extern GLint (*glGetAttribLocation)(GLuint, const GLchar*);
extern GLint (*glGetUniformLocation)(GLuint, const GLchar*);
extern void (*glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
extern void (*glUniform1i)(GLint, GLint);

extern void (*glEnableVertexAttribArray)(GLuint);
extern void (*glDisableVertexAttribArray)(GLuint);
extern void (*glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

// Loads all of the above via SDL_GL_GetProcAddress. Call once, right after
// creating the GL context. Returns false if any function failed to load.
bool loadGLExtensions();
