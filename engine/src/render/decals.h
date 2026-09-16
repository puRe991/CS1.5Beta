#pragma once

#include <string>
#include <vector>

#include "glext.h"
#include "../mat4.h"
#include "../assets/wad.h"

// Persistent world-space decals (bullet holes, blood splatter) — flat,
// alpha-blended quads glued to a surface at the point and normal of a
// hitscan trace, using the real GoldSrc decal textures (decals.wad's
// "{shot*"/"{blood*" alpha-keyed lumps) rather than the plain dark dot
// this replaces.
class DecalSystem {
public:
    // Loads every texture name in `names` from one WAD (decals.wad) up
    // front; spawn() then picks uniformly among whichever group (bullet
    // hole vs. blood) it's given. A name that fails to decode is skipped,
    // so spawn() just has fewer variants rather than crashing.
    //
    // tintR/G/B colors the decal: a decal WAD lump's palette isn't real
    // color at all, just a grayscale ramp used as a per-pixel alpha mask
    // (index 0 = transparent background, 255 = fully opaque), so there's
    // no actual color to decode — the caller supplies one instead (e.g.
    // near-black for a bullet hole, dark red for blood).
    std::vector<int> loadGroup(const std::string& wadPath, const std::vector<std::string>& names,
                               uint8_t tintR, uint8_t tintG, uint8_t tintB);

    // Sticks a decal of a random texture from `group` (as returned by
    // loadGroup) onto the surface at `pos` with the given outward-facing
    // `normal`, sized `size` world units across. Oldest decals are evicted
    // once the total count exceeds a fixed cap, same as the old impact
    // marks did, so this can't grow unbounded over a long round.
    void spawn(const std::vector<int>& group, Vec3f pos, Vec3f normal, float size);

    // Expects the caller's GL_PROJECTION/GL_MODELVIEW to already be the
    // world proj/view (same convention as particles/impact marks).
    void draw() const;

    ~DecalSystem();

private:
    struct Decal {
        Vec3f pos, normal, right, up;
        float size;
        GLuint texId;
    };

    std::vector<GLuint> textures_;
    std::vector<Decal> decals_;
};
