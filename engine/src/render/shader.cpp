#include "shader.h"

#include <cstdio>
#include <vector>

namespace {

GLuint compile(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, 0x8B84 /*GL_INFO_LOG_LENGTH*/, &len);
        std::vector<char> log(len > 0 ? len : 1);
        glGetShaderInfoLog(shader, (GLsizei)log.size(), nullptr, log.data());
        std::fprintf(stderr, "shader compile error: %s\n", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

bool Shader::load(const char* vertexSrc, const char* fragmentSrc) {
    GLuint vs = compile(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs) return false;

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program_, 0x8B84 /*GL_INFO_LOG_LENGTH*/, &len);
        std::vector<char> log(len > 0 ? len : 1);
        glGetProgramInfoLog(program_, (GLsizei)log.size(), nullptr, log.data());
        std::fprintf(stderr, "shader link error: %s\n", log.data());
        return false;
    }
    return true;
}

void Shader::use() const {
    glUseProgram(program_);
}

void Shader::setMat4(const char* name, const Mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(program_, name), 1, GL_FALSE, m.m);
}

void Shader::setInt(const char* name, int value) const {
    glUniform1i(glGetUniformLocation(program_, name), value);
}

GLint Shader::attribLocation(const char* name) const {
    return glGetAttribLocation(program_, name);
}
