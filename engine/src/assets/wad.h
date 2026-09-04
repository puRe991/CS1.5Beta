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

private:
    struct LumpInfo {
        uint32_t filePos;
        uint32_t diskSize;
    };

    std::string path_;
    std::vector<LumpInfo> lumps_;
    std::vector<std::string> lumpNames_;
};
