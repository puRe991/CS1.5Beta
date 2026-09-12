#include "decals.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr size_t kMaxDecals = 256;

GLuint uploadDecalTexture(const WadTexture& tex) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.rgba.data());
    return id;
}

} // namespace

std::vector<int> DecalSystem::loadGroup(const std::string& wadPath, const std::vector<std::string>& names,
                                         uint8_t tintR, uint8_t tintG, uint8_t tintB) {
    WadFile wad;
    std::vector<int> group;
    if (!wad.open(wadPath)) return group;

    for (const auto& name : names) {
        for (size_t i = 0; i < wad.textureCount(); ++i) {
            if (wad.textureName(i) != name) continue;
            WadTexture tex;
            if (wad.decodeDecalTexture(i, tintR, tintG, tintB, tex)) {
                textures_.push_back(uploadDecalTexture(tex));
                group.push_back((int)textures_.size() - 1);
            }
            break;
        }
    }
    return group;
}

void DecalSystem::spawn(const std::vector<int>& group, Vec3f pos, Vec3f normal, float size) {
    if (group.empty()) return;

    // Any vector not parallel to normal works as a seed for a basis;
    // world-up unless the surface is itself near-horizontal (a floor or
    // ceiling), in which case fall back to world-X.
    Vec3f seed = std::abs(normal.z) < 0.9f ? Vec3f{0, 0, 1} : Vec3f{1, 0, 0};
    Vec3f right = normalize(cross(seed, normal));
    Vec3f up = cross(normal, right);

    Decal d;
    d.pos = pos;
    d.normal = normal;
    d.right = right;
    d.up = up;
    d.size = size;
    d.texId = textures_[(size_t)group[std::rand() % group.size()]];

    if (decals_.size() >= kMaxDecals) decals_.erase(decals_.begin());
    decals_.push_back(d);
}

void DecalSystem::draw() const {
    if (decals_.empty()) return;

    // Nudged off the surface along its normal so it doesn't z-fight with
    // the wall/floor it's glued to, same trick the old impact dots used.
    constexpr float kOffset = 0.5f;
    constexpr float kHalf = 0.5f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D);

    for (const auto& d : decals_) {
        Vec3f base{d.pos.x + d.normal.x * kOffset, d.pos.y + d.normal.y * kOffset, d.pos.z + d.normal.z * kOffset};
        float h = d.size * kHalf;

        glBindTexture(GL_TEXTURE_2D, d.texId);
        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0, 0); glVertex3f(base.x + (-d.right.x - d.up.x) * h, base.y + (-d.right.y - d.up.y) * h, base.z + (-d.right.z - d.up.z) * h);
        glTexCoord2f(1, 0); glVertex3f(base.x + ( d.right.x - d.up.x) * h, base.y + ( d.right.y - d.up.y) * h, base.z + ( d.right.z - d.up.z) * h);
        glTexCoord2f(1, 1); glVertex3f(base.x + ( d.right.x + d.up.x) * h, base.y + ( d.right.y + d.up.y) * h, base.z + ( d.right.z + d.up.z) * h);
        glTexCoord2f(0, 1); glVertex3f(base.x + (-d.right.x + d.up.x) * h, base.y + (-d.right.y + d.up.y) * h, base.z + (-d.right.z + d.up.z) * h);
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

DecalSystem::~DecalSystem() {
    for (GLuint t : textures_) glDeleteTextures(1, &t);
}
