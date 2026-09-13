#include "spr.h"

#include <cstdio>
#include <cstring>

namespace {

// Matches the HL SDK's dsprite_t exactly (40 bytes).
struct SpriteHeader {
    int32_t ident;   // "IDSP"
    int32_t version; // 2
    int32_t type;
    int32_t texFormat;
    float boundingRadius;
    int32_t width, height; // max frame dimensions
    int32_t numFrames;
    float beamLength;
    int32_t synctype;
};

// Matches dspriteframe_t: origin is where to offset the quad from its draw
// position (e.g. -width/2 to center it), followed by width*height bytes of
// palette indices.
struct SpriteFrameHeader {
    int32_t originX, originY;
    int32_t width, height;
};

} // namespace

bool SprModel::load(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    std::fseek(f, 0, SEEK_END);
    long fileSize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> data(fileSize);
    if (std::fread(data.data(), 1, fileSize, f) != (size_t)fileSize) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    if (data.size() < sizeof(SpriteHeader)) return false;
    const SpriteHeader* hdr = reinterpret_cast<const SpriteHeader*>(data.data());
    if (std::memcmp(&hdr->ident, "IDSP", 4) != 0 || hdr->version != 2) return false;

    orientation_ = (hdr->type >= 0 && hdr->type <= 4) ? (SprOrientation)hdr->type : SprOrientation::Parallel;
    renderMode_ = (hdr->texFormat >= 0 && hdr->texFormat <= 3) ? (SprRenderMode)hdr->texFormat : SprRenderMode::Normal;

    size_t off = sizeof(SpriteHeader);
    if (off + 2 > data.size()) return false;
    int16_t numColors = *reinterpret_cast<const int16_t*>(data.data() + off);
    off += 2;

    if (off + (size_t)numColors * 3 > data.size()) return false;
    const uint8_t* palette = data.data() + off;
    off += (size_t)numColors * 3;

    frames_.clear();
    frames_.reserve(hdr->numFrames);
    for (int32_t i = 0; i < hdr->numFrames; ++i) {
        if (off + 4 > data.size()) return false;
        int32_t frameType = *reinterpret_cast<const int32_t*>(data.data() + off);
        off += 4;
        // Group frames (animated sub-sequences with per-frame display
        // intervals) don't appear in any CS1.5 sprite this was tested
        // against — bail out rather than mis-parse an unknown layout.
        if (frameType != 0) return false;

        if (off + sizeof(SpriteFrameHeader) > data.size()) return false;
        const SpriteFrameHeader* fh = reinterpret_cast<const SpriteFrameHeader*>(data.data() + off);
        off += sizeof(SpriteFrameHeader);

        size_t pixelCount = (size_t)fh->width * fh->height;
        if (off + pixelCount > data.size()) return false;
        const uint8_t* indices = data.data() + off;
        off += pixelCount;

        SprFrame frame;
        frame.width = (uint32_t)fh->width;
        frame.height = (uint32_t)fh->height;
        frame.originX = fh->originX;
        frame.originY = fh->originY;
        frame.rgba.resize(pixelCount * 4);
        for (size_t p = 0; p < pixelCount; ++p) {
            uint8_t idx = indices[p];
            uint8_t r = palette[idx * 3 + 0];
            uint8_t g = palette[idx * 3 + 1];
            uint8_t b = palette[idx * 3 + 2];
            uint8_t a = 255;
            switch (renderMode_) {
                case SprRenderMode::AlphaTest:
                    a = (idx == 255) ? 0 : 255;
                    break;
                case SprRenderMode::IndexAlpha:
                    // Palette is a grayscale alpha ramp (palette[idx] ==
                    // (idx,idx,idx) in every sprite checked) — use it as
                    // alpha over a white base color, since there's no
                    // per-entity render-color tinting yet to modulate it.
                    a = idx;
                    r = g = b = 255;
                    break;
                case SprRenderMode::Normal:
                case SprRenderMode::Additive:
                default:
                    a = 255;
                    break;
            }
            frame.rgba[p * 4 + 0] = r;
            frame.rgba[p * 4 + 1] = g;
            frame.rgba[p * 4 + 2] = b;
            frame.rgba[p * 4 + 3] = a;
        }
        frames_.push_back(std::move(frame));
    }

    return true;
}
