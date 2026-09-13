#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Decoded GoldSrc texture (mip level 0 only), always RGBA8.
struct WadTexture {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba; // width * height * 4
};

// Reader for WAD3 archives (GoldSrc texture packages).
class WadFile {
public:
    // Opens a .wad3 file and reads its lump directory. Returns false on failure.
    bool open(const std::string& path);

    size_t textureCount() const { return lumpNames_.size(); }
    const std::string& textureName(size_t index) const { return lumpNames_[index]; }

    // Decodes lump `index` (mip level 0) into an RGBA8 texture.
    bool decodeTexture(size_t index, WadTexture& out) const;

    // Decodes lump `index` as a decal (decals.wad's "{shot*"/"{blood*"
    // etc. lumps): unlike a masked world texture, a decal's palette isn't
    // real color at all — it's a grayscale ramp (palette[i] is always
    // (255-i,255-i,255-i) in every decal checked) used purely as a
    // per-pixel alpha mask, with index 0 = fully transparent background
    // and higher indices = more opaque. There's no color to recover from
    // that ramp, so the caller supplies one (e.g. dark red for blood).
    bool decodeDecalTexture(size_t index, uint8_t tintR, uint8_t tintG, uint8_t tintB, WadTexture& out) const;

private:
    struct LumpInfo {
        uint32_t filePos;
        uint32_t diskSize;
    };

    std::string path_;
    std::vector<LumpInfo> lumps_;
    std::vector<std::string> lumpNames_;
};
