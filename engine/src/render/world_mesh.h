#pragma once

#include <vector>

#include "glext.h"
#include "shader.h"
#include "../assets/bsp.h"

// Triangulates every BSP face once at load time and uploads it into a single
// VBO, grouped by texture, so the main render loop is a handful of
// glDrawArrays calls through a GLSL shader instead of one glBegin/glEnd
// polygon per face. Requires loadGLExtensions() to have already succeeded.
class WorldMesh {
public:
    void build(const BspMap& map, const std::vector<GLuint>& texIds);
    void draw(const Shader& shader) const;
    ~WorldMesh();

private:
    struct DrawGroup {
        GLuint texId;
        GLsizei start;
        GLsizei count;
    };

    GLuint vbo_ = 0;
    std::vector<DrawGroup> groups_;
};
