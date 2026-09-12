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

// A named attachment point (e.g. a weapon's muzzle, or a player model's
// weapon-hand bone), given in the same world/model space as triangles() —
// i.e. already transformed by its bone's bind-pose, not bone-local.
struct MdlAttachment {
    std::string name;
    float x = 0, y = 0, z = 0;
};

// One animation sequence (mstudioseqdesc_t): "idle1", "walk", "shoot1", etc.
struct MdlSequence {
    std::string name;
    float fps = 30.0f;
    int numFrames = 1;
    bool looping = false;
    // >1 for a directional-blend sequence (e.g. a 9-way aim/shoot pose set
    // spanning look-up to look-down) — see pose()'s `blend` parameter.
    int numBlends = 1;
};

// Loads a GoldSrc Studio Model (.mdl, version 10) into a flat triangle soup,
// posed at its reference/bind pose by default (triangles()) — this is the
// pose the model's vertex data was authored against, so it already looks
// correct standing still. pose() re-skins the mesh at an arbitrary point in
// a named sequence's animation for real playback.
class MdlModel {
public:
    bool load(const std::string& path);

    const std::vector<MdlTriangle>& triangles() const { return triangles_; }
    const std::vector<MdlTexture>& textures() const { return textures_; }
    const std::vector<MdlAttachment>& attachments() const { return attachments_; }

    // Convenience lookup by name (e.g. "muzzle"); returns nullptr if absent.
    const MdlAttachment* findAttachment(const std::string& name) const;

    const std::vector<MdlSequence>& sequences() const { return sequences_; }
    int findSequence(const std::string& name) const;

    // Computes the mesh posed at a fractional frame (0..numFrames-1,
    // interpolated) within one sequence, by decoding that sequence's
    // per-bone compressed animation tracks and re-skinning every vertex.
    // Not cached — call once per rendered frame, not per vertex access.
    //
    // `blend` (0..1) picks a point across a directional-blend sequence's
    // numBlends variants (e.g. a 9-way aim/shoot set: 0 = full look-up,
    // 1 = full look-down, 0.5 = level), linearly interpolated between the
    // two nearest ones — a real analog of the engine's own pitch-driven
    // blend controller, just fed a normalized value instead of a raw bone
    // controller angle since there's no controller system here. Ignored
    // (and free to omit) for the numBlends == 1 sequences that make up
    // most of this game's asset set.
    std::vector<MdlTriangle> pose(int sequenceIndex, float frame, float blend = 0.5f) const;

private:
    std::vector<MdlTriangle> triangles_;
    std::vector<MdlTexture> textures_;
    std::vector<MdlAttachment> attachments_;
    std::vector<MdlSequence> sequences_;
    std::vector<uint8_t> fileData_; // kept alive so pose() can re-read raw anim data on demand
};
