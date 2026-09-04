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

// Bounding box of one BSP submodel (world geometry group). Brush entities
// (func_bomb_target, func_buyzone, func_door, ...) reference one of these by
// index via their "model" key, formatted as "*N".
struct BspModelBounds {
    Vec3 mins, maxs;
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
    const std::vector<BspModelBounds>& models() const { return models_; }

    // Parses a brush entity's "model" key (e.g. "*16") into an index into
    // models(). Returns -1 if the entity has no model key or it's malformed.
    static int modelIndexFor(const BspEntity& ent);

    // True if `point` (a player origin) sits inside solid geometry, tested against
    // hull 1 (the standard player-sized box hull), same as the original engine's
    // SV_HullPointContents — no separate box-vs-geometry test needed since the
    // clipnode planes are already offset to account for the hull's extents.
    bool pointInSolid(Vec3 point) const;

    // Steps from start toward end (in fixed increments) until it enters solid
    // geometry or reaches the end. Returns true and sets outHit on a hit.
    // Deliberately simple (not a proper swept hull trace) — good enough for
    // bullet impact marks; the player hull's box inflation means it stops
    // slightly before the true wall face, a known simplification.
    bool traceLine(Vec3 start, Vec3 end, Vec3& outHit) const;

private:
    struct Plane {
        float nx, ny, nz, dist;
    };
    struct ClipNode {
        int32_t planeNum;
        int16_t children[2];
    };

    std::vector<BspFace> faces_;
    std::vector<BspTexture> textures_;
    std::vector<BspEntity> entities_;
    std::vector<Plane> planes_;
    std::vector<ClipNode> clipNodes_;
    std::vector<BspModelBounds> models_;
    int32_t hull1HeadNode_ = -1;

    void parseEntities(const std::string& entityText);
};
