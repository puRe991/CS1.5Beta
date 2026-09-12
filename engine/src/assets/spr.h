#pragma once

#include <cstdint>
#include <string>
#include <vector>

// GoldSrc sprite orientation (dsprite_t::type) — how the engine is meant to
// rotate the quad to face the camera when drawing it.
enum class SprOrientation {
    ParallelUpright = 0, // rotates around Z only, no pitch/roll (billboarded on Z axis)
    FacingUpright = 1,   // like ParallelUpright but always faces the viewer's XY position
    Parallel = 2,        // fully camera-facing (classic billboard) — most effects use this
    Oriented = 3,        // fixed orientation in world space, doesn't face the camera at all
    ParallelOriented = 4,
};

// GoldSrc sprite render/blend mode (dsprite_t::texFormat) — determines how
// the palette maps to RGBA below.
enum class SprRenderMode {
    Normal = 0,     // opaque
    Additive = 1,   // opaque RGB, meant to be drawn with additive blending
    IndexAlpha = 2, // palette is a grayscale ramp used as a per-pixel alpha mask (white RGB)
    AlphaTest = 3,  // palette index 255 is a transparency key (alpha 0), else opaque
};

struct SprFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    // Frame origin (crosshair convention: how far to offset the quad from
    // its draw position, e.g. -width/2 to center it) — from dspriteframe_t.
    int32_t originX = 0;
    int32_t originY = 0;
    std::vector<uint8_t> rgba;
};

// Loads a GoldSrc sprite (.spr, version 2) — the format used for muzzle
// flashes, smoke puffs, and some flat HUD elements (radar, crosshair
// direction arrows). Palettized like .wad/.mdl, decoded to RGBA frames here
// so callers can upload them exactly like any other texture.
class SprModel {
public:
    bool load(const std::string& path);

    SprOrientation orientation() const { return orientation_; }
    SprRenderMode renderMode() const { return renderMode_; }
    const std::vector<SprFrame>& frames() const { return frames_; }

private:
    SprOrientation orientation_ = SprOrientation::Parallel;
    SprRenderMode renderMode_ = SprRenderMode::Normal;
    std::vector<SprFrame> frames_;
};
