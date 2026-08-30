#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// One renderable polygon: a fan of world-space vertices sharing one texture.
struct BspFace {
    std::vector<Vec3> vertices;   // fan order, world space (Quake coords)
    std::vector<float> texCoords; // 2 per vertex, in texel units (divide by tex size)
    int textureIndex = -1;        // index into BspMap::textures, or -1 if unresolved
};

struct BspTexture {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba; // empty if not resolved (external wad missing)
};

struct BspEntity {
    // Raw key/value pairs, e.g. "classname" -> "info_player_start"
    std::vector<std::pair<std::string, std::string>> pairs;
    const std::string* get(const std::string& key) const;
};

// Parsed BSP v30 (GoldSrc) map: geometry + resolved textures + entities.
class BspMap {
public:
    // externalWadDirs: directories to search for WAD files referenced by
    // worldspawn's "wad" key, used when a texture isn't embedded in the BSP.
    bool load(const std::string& path, const std::vector<std::string>& externalWadDirs);

    const std::vector<BspFace>& faces() const { return faces_; }
    const std::vector<BspTexture>& textures() const { return textures_; }
    const std::vector<BspEntity>& entities() const { return entities_; }

private:
    std::vector<BspFace> faces_;
    std::vector<BspTexture> textures_;
    std::vector<BspEntity> entities_;

    void parseEntities(const std::string& entityText);
};
