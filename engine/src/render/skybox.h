#pragma once

#include <string>

#include "glext.h"
#include "shader.h"
#include "../mat4.h"

// Renders the 6-image GoldSrc skybox (envDir/<skyname>{up,dn,lf,rt,ft,bk}.tga)
// as a cube centered on the camera, drawn with depth testing off so it
// always sits "behind" everything else without needing sorting.
class Skybox {
public:
    // Returns false (leaving the skybox absent, not fatal) if the skyname is
    // empty or any of the 6 face images can't be loaded.
    bool load(const std::string& envDir, const std::string& skyname);

    void draw(const Shader& shader, Vec3f eyePos) const;
    bool valid() const { return valid_; }

    ~Skybox();

private:
    GLuint faceTex_[6] = {0, 0, 0, 0, 0, 0}; // up, dn, lf, rt, ft, bk
    GLuint vbo_ = 0;
    bool valid_ = false;
};
