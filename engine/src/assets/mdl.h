#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct MdlVertex {
    float x, y, z;
    float u, v; // texel units, like BspFace (divide by texture size before use)
};

struct MdlTriangle {
    MdlVertex a, b, c;
    int textureIndex = -1;
};

struct MdlTexture {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba;
};

// Loads a GoldSrc Studio Model (.mdl, version 10) into a flat triangle soup,
// posed at its reference/bind pose (no animation playback yet — this is the
// pose the model's vertex data was authored against, so it already looks
// correct standing still).
class MdlModel {
public:
    bool load(const std::string& path);

    const std::vector<MdlTriangle>& triangles() const { return triangles_; }
    const std::vector<MdlTexture>& textures() const { return textures_; }

private:
    std::vector<MdlTriangle> triangles_;
    std::vector<MdlTexture> textures_;
};
