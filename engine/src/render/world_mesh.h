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
    // lightmapSampler is the texture unit index (e.g. 1) the shader's
    // "uLightmap" uniform should be bound to; the base texture stays on unit 0.
    void build(const BspMap& map, const std::vector<GLuint>& texIds);

    // visibleFaces: from BspMap::computeVisibleFaces(), indexed exactly
    // like the BspMap::faces() this mesh was built from. Empty (the
    // default) draws every face, same as before PVS culling existed —
    // pass it whenever the caller has no usable visibility data (outside
    // the map, or a map with no compiled PVS).
    void draw(const Shader& shader, const std::vector<bool>& visibleFaces = {}) const;
    ~WorldMesh();

private:
    // One face's vertex range within its texture group's VBO block, so
    // draw() can skip/emit sub-ranges by PVS visibility instead of always
    // drawing the whole group in one call.
    struct FaceRange {
        int faceIndex;
        GLsizei start, count;
    };
    struct DrawGroup {
        GLuint texId;
        GLsizei start, count; // the group's full range, used when visibleFaces is empty
        std::vector<FaceRange> faceRanges; // sorted by leaf for good coalescing
    };

    GLuint vbo_ = 0;
    GLuint lightmapAtlas_ = 0;
    std::vector<DrawGroup> groups_;
};
