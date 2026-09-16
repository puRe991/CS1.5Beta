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

    // Lightmap (style 0 only — no animated/switchable light styles yet).
    // lightmapTexCoords are in luxel space (0..lightmapWidth/Height), 2 per
    // vertex, matching the vertex/texCoords order. lightmapRGB is empty if
    // the face has no lightmap data (e.g. a fullbright/sky surface).
    std::vector<float> lightmapTexCoords;
    uint32_t lightmapWidth = 0, lightmapHeight = 0;
    std::vector<uint8_t> lightmapRGB; // lightmapWidth * lightmapHeight * 3
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
    // Per-hull collision tree roots for this submodel specifically (same
    // hull numbering as BspMap::pointInSolidHull) — lets a brush entity
    // (func_door, func_plat, func_breakable, ...) be tested for collision
    // on its own, independently of the worldspawn model's tree in
    // headNodes_. -1 = no tree for that hull.
    int32_t headNode[4] = {-1, -1, -1, -1};
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

    // Same traversal as pointInSolid, but also reports the last clip plane
    // tested before reaching a solid leaf (see traceLine's outNormal).
    bool pointInSolid(Vec3 point, Vec3& outPlaneNormal) const;

    // Same test against any of the map's 4 precompiled collision hulls —
    // each hull's clipnode planes are already offset for a specific box
    // size, so the point tested is always the same origin regardless of
    // hull, only the effective box size changes:
    //   0 = point (zero-size, used for tracelines)
    //   1 = standard standing player box
    //   2 = "large" box (bigger monsters in the original engine)
    //   3 = crouching player box
    // Returns false (never solid) if the map has no data for that hull.
    bool pointInSolidHull(Vec3 point, int hull, Vec3* outPlaneNormal = nullptr) const;

    // Same test, but against one specific submodel's own collision tree
    // (models()[modelIndex].headNode) instead of the worldspawn model's.
    // `point` must already be in that submodel's local/rest space — for a
    // brush entity that has moved by some offset, pass (point - offset) so
    // the test lines up with the tree's baked-at-compile-time position.
    // Used for func_door/func_plat/func_breakable collision (see
    // brush_entities.h), which move independently of the static world.
    bool pointInSolidModel(Vec3 point, int modelIndex, int hull, Vec3* outPlaneNormal = nullptr) const;

    // Steps from start toward end (in fixed increments) until it enters solid
    // geometry or reaches the end. Returns true and sets outHit on a hit.
    // Deliberately simple (not a proper swept hull trace) — good enough for
    // bullet impact marks; the player hull's box inflation means it stops
    // slightly before the true wall face, a known simplification.
    // outNormal (if non-null) is set to the clip plane that classified the
    // hit point as solid — an approximation of the true surface normal
    // (exact for the common case of an axis-aligned wall/floor brush), used
    // to orient decals against the surface.
    bool traceLine(Vec3 start, Vec3 end, Vec3& outHit, Vec3* outNormal = nullptr) const;

    // PVS-based visibility culling: which faces() are potentially visible
    // from a given viewpoint, indexed exactly like faces(). Returns an
    // empty vector if the map has no usable visibility data (e.g. the
    // point resolved to the outside/solid leaf, or the map was compiled
    // without one) — callers should treat that as "draw everything".
    std::vector<bool> computeVisibleFaces(Vec3 viewPos) const;

    // Which BSP leaf each face() belongs to (-1 if unknown), same
    // indexing as faces() — used to sort draw batches for locality so
    // PVS-culled ranges coalesce into fewer, larger draw calls.
    const std::vector<int32_t>& faceLeafIndices() const { return faceLeaf_; }

    // Which submodel (models() index) each faces() entry belongs to, same
    // indexing as faces() — 0 for ordinary static world geometry, >0 for
    // faces that belong to a brush entity's model and so should be drawn
    // with that entity's current offset instead of statically.
    const std::vector<int32_t>& faceModelIndices() const { return faceModel_; }

private:
    struct Plane {
        float nx, ny, nz, dist;
    };
    struct ClipNode {
        int32_t planeNum;
        int16_t children[2];
    };
    struct RenderNode {
        int32_t planeNum;
        int32_t children[2]; // widened from the file's int16_t; negative = -(leaf)-1
    };
    struct Leaf {
        int32_t visOfs; // -1 = no vis data for this leaf
        uint16_t firstMarkSurface, numMarkSurfaces;
    };

    std::vector<BspFace> faces_;
    std::vector<BspTexture> textures_;
    std::vector<BspEntity> entities_;
    std::vector<Plane> planes_;
    std::vector<ClipNode> clipNodes_;
    std::vector<BspModelBounds> models_;
    int32_t headNodes_[4] = {-1, -1, -1, -1}; // model 0's per-hull collision tree roots

    std::vector<RenderNode> nodes_;
    std::vector<Leaf> leafs_;
    std::vector<uint16_t> markSurfaces_;
    std::vector<uint8_t> visData_;
    std::vector<int32_t> rawToCompactFace_; // raw DFace index -> faces_ index, or -1 if culled
    std::vector<int32_t> faceLeaf_;         // faces_ index -> owning leaf, or -1
    std::vector<int32_t> faceModel_;        // faces_ index -> owning submodel (0 = world)
    int32_t renderHeadNode_ = -1;

    int32_t findLeaf(Vec3 point) const;
    bool walkClipTree(Vec3 point, int32_t headNode, Vec3* outPlaneNormal) const;
    void parseEntities(const std::string& entityText);
};
