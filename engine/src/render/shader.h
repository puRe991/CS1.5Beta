#pragma once

#include <string>

#include "glext.h"
#include "../mat4.h"

// Thin wrapper around a linked GLSL vertex+fragment program. Requires
// loadGLExtensions() to have already succeeded.
class Shader {
public:
    // Returns false (and logs to stderr) on a compile/link failure.
    bool load(const char* vertexSrc, const char* fragmentSrc);

    void use() const;
    void setMat4(const char* name, const Mat4& m) const;
    void setInt(const char* name, int value) const;

    GLint attribLocation(const char* name) const;

    GLuint program() const { return program_; }

private:
    GLuint program_ = 0;
};
