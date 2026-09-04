#include "wad.h"

#include <cstdio>
#include <cstring>

namespace {

struct WadHeader {
    char magic[4]; // "WAD3"
    int32_t numLumps;
    int32_t infoTableOfs;
};

struct WadLumpInfo {
    int32_t filePos;
    int32_t diskSize;
    int32_t size;
    char type;
    char compression;
    int16_t pad;
    char name[16];
};

constexpr char kMipTexType = 0x43;

struct MipTexHeader {
    char name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[4]; // relative to start of MipTexHeader, mip levels 0..3
};

} // namespace

bool WadFile::open(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    WadHeader header;
    if (std::fread(&header, sizeof(header), 1, f) != 1) {
        std::fclose(f);
        return false;
    }
    if (std::memcmp(header.magic, "WAD3", 4) != 0) {
        std::fclose(f);
        return false;
    }

    std::fseek(f, header.infoTableOfs, SEEK_SET);

    lumps_.clear();
    lumpNames_.clear();
    lumps_.reserve(header.numLumps);
    lumpNames_.reserve(header.numLumps);

    for (int32_t i = 0; i < header.numLumps; ++i) {
        WadLumpInfo lump;
        if (std::fread(&lump, sizeof(lump), 1, f) != 1) break;
        if (lump.type != kMipTexType) continue; // only textures are supported for now

        LumpInfo info;
        info.filePos = (uint32_t)lump.filePos;
        info.diskSize = (uint32_t)lump.diskSize;
        lumps_.push_back(info);
        lumpNames_.emplace_back(lump.name, strnlen(lump.name, sizeof(lump.name)));
    }

    std::fclose(f);
    path_ = path;
    return true;
}

bool WadFile::decodeTexture(size_t index, WadTexture& out) const {
    if (index >= lumps_.size()) return false;
    const LumpInfo& lump = lumps_[index];

    FILE* f = std::fopen(path_.c_str(), "rb");
    if (!f) return false;

    std::fseek(f, (long)lump.filePos, SEEK_SET);

    MipTexHeader mip;
    if (std::fread(&mip, sizeof(mip), 1, f) != 1) {
        std::fclose(f);
        return false;
    }

    if (mip.width == 0 || mip.height == 0 || mip.offsets[0] == 0) {
        std::fclose(f);
        return false;
    }

    // Mip level 0 pixel indices.
    std::vector<uint8_t> indices(mip.width * mip.height);
    std::fseek(f, (long)(lump.filePos + mip.offsets[0]), SEEK_SET);
    if (std::fread(indices.data(), 1, indices.size(), f) != indices.size()) {
        std::fclose(f);
        return false;
    }

    // Palette sits right after the smallest mip level (mip 3) plus a 2-byte color count.
    size_t mip3Size = (mip.width / 8) * (mip.height / 8);
    long paletteOffset = (long)(lump.filePos + mip.offsets[3] + mip3Size + 2);
    std::fseek(f, paletteOffset, SEEK_SET);

    uint8_t palette[256 * 3];
    if (std::fread(palette, 1, sizeof(palette), f) != sizeof(palette)) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    bool transparent = !lumpNames_[index].empty() && lumpNames_[index][0] == '{';

    out.name.assign(mip.name, strnlen(mip.name, sizeof(mip.name)));
    out.width = mip.width;
    out.height = mip.height;
    out.rgba.resize((size_t)mip.width * mip.height * 4);

    for (size_t i = 0; i < indices.size(); ++i) {
        uint8_t paletteIndex = indices[i];
        uint8_t r = palette[paletteIndex * 3 + 0];
        uint8_t g = palette[paletteIndex * 3 + 1];
        uint8_t b = palette[paletteIndex * 3 + 2];
        uint8_t a = (transparent && paletteIndex == 255) ? 0 : 255;

        out.rgba[i * 4 + 0] = r;
        out.rgba[i * 4 + 1] = g;
        out.rgba[i * 4 + 2] = b;
        out.rgba[i * 4 + 3] = a;
    }

    return true;
}
