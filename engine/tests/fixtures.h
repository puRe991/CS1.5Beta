#pragma once

// Helpers to build minimal, valid in-memory binary fixtures for the asset
// file formats (PAK, WAD3, BSP v30, Studio MDL v10), so the parsers in
// engine/src/assets can be unit-tested without shipping real game data.
//
// The struct layouts here mirror the on-disk formats exactly as consumed by
// the parsers (see assets/pak.cpp, wad.cpp, bsp.cpp, mdl.cpp) — duplicated
// here deliberately, since these are fixed file-format layouts, not
// application logic to share.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fixtures {

inline std::string tempFilePath(const std::string& suffix) {
    static int counter = 0;
    auto dir = std::filesystem::temp_directory_path();
    std::string name = "cs15_test_" + std::to_string(++counter) + suffix;
    return (dir / name).string();
}

inline bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    return ok;
}

template <typename T>
void append(std::vector<uint8_t>& buf, const T& value) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
    buf.insert(buf.end(), p, p + sizeof(T));
}

inline void appendBytes(std::vector<uint8_t>& buf, const void* data, size_t n) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    buf.insert(buf.end(), p, p + n);
}

inline void setName(char* dst, size_t dstSize, const char* src) {
    std::memset(dst, 0, dstSize);
    std::strncpy(dst, src, dstSize - 1);
}

// ---------------------------------------------------------------------------
// PAK (Quake-style archive, see assets/pak.cpp)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct PakHeader {
    char magic[4];
    int32_t dirOffset;
    int32_t dirLength;
};
struct PakDirEntry {
    char name[56];
    int32_t filePos;
    int32_t fileLen;
};
#pragma pack(pop)

struct PakBuilderEntry {
    std::string name;
    std::vector<uint8_t> data;
};

// Builds a .pak file on disk containing the given entries; returns its path.
inline std::string buildPak(const std::vector<PakBuilderEntry>& entries) {
    std::vector<uint8_t> body; // concatenated file contents
    std::vector<PakDirEntry> dir;
    for (const auto& e : entries) {
        PakDirEntry d{};
        setName(d.name, sizeof(d.name), e.name.c_str());
        d.filePos = (int32_t)(sizeof(PakHeader) + body.size());
        d.fileLen = (int32_t)e.data.size();
        dir.push_back(d);
        body.insert(body.end(), e.data.begin(), e.data.end());
    }

    PakHeader header{};
    std::memcpy(header.magic, "PACK", 4);
    header.dirOffset = (int32_t)(sizeof(PakHeader) + body.size());
    header.dirLength = (int32_t)(dir.size() * sizeof(PakDirEntry));

    std::vector<uint8_t> file;
    append(file, header);
    file.insert(file.end(), body.begin(), body.end());
    for (const auto& d : dir) append(file, d);

    std::string path = tempFilePath(".pak");
    writeFile(path, file);
    return path;
}

// ---------------------------------------------------------------------------
// WAD3 (GoldSrc texture package, see assets/wad.cpp)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct WadHeader {
    char magic[4];
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
struct WadMipTexHeader {
    char name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[4];
};
#pragma pack(pop)

constexpr char kWadMipTexType = 0x43;

// Builds a single-texture WAD3 file: a solid-color `width`x`height` texture
// named `texName`, using only mip level 0 (mips 1-3 are zero-filled, same
// size formula the decoder expects). Returns the file path.
inline std::string buildWadWithSolidTexture(const std::string& texName, uint32_t width,
                                             uint32_t height, uint8_t paletteIndex,
                                             uint8_t r, uint8_t g, uint8_t b) {
    // Layout inside the miptex lump: header, mip0..mip3 pixel indices, 2-byte
    // color count, then a 256-entry RGB palette (matches decodeTexture's math).
    WadMipTexHeader mip{};
    setName(mip.name, sizeof(mip.name), texName.c_str());
    mip.width = width;
    mip.height = height;

    size_t mip0Size = (size_t)width * height;
    size_t mip1Size = (width / 2) * (height / 2);
    size_t mip2Size = (width / 4) * (height / 4);
    size_t mip3Size = (width / 8) * (height / 8);

    mip.offsets[0] = sizeof(WadMipTexHeader);
    mip.offsets[1] = mip.offsets[0] + (uint32_t)mip0Size;
    mip.offsets[2] = mip.offsets[1] + (uint32_t)mip1Size;
    mip.offsets[3] = mip.offsets[2] + (uint32_t)mip2Size;

    std::vector<uint8_t> lump;
    append(lump, mip);
    lump.insert(lump.end(), mip0Size, paletteIndex);
    lump.insert(lump.end(), mip1Size, paletteIndex);
    lump.insert(lump.end(), mip2Size, paletteIndex);
    lump.insert(lump.end(), mip3Size, paletteIndex);
    append(lump, uint16_t{256}); // color count
    uint8_t palette[256 * 3] = {0};
    palette[paletteIndex * 3 + 0] = r;
    palette[paletteIndex * 3 + 1] = g;
    palette[paletteIndex * 3 + 2] = b;
    appendBytes(lump, palette, sizeof(palette));

    WadLumpInfo lumpInfo{};
    lumpInfo.filePos = sizeof(WadHeader);
    lumpInfo.diskSize = (int32_t)lump.size();
    lumpInfo.size = (int32_t)lump.size();
    lumpInfo.type = kWadMipTexType;
    lumpInfo.compression = 0;
    lumpInfo.pad = 0;
    setName(lumpInfo.name, sizeof(lumpInfo.name), texName.c_str());

    WadHeader header{};
    std::memcpy(header.magic, "WAD3", 4);
    header.numLumps = 1;
    header.infoTableOfs = (int32_t)(sizeof(WadHeader) + lump.size());

    std::vector<uint8_t> file;
    append(file, header);
    file.insert(file.end(), lump.begin(), lump.end());
    append(file, lumpInfo);

    std::string path = tempFilePath(".wad");
    writeFile(path, file);
    return path;
}

// ---------------------------------------------------------------------------
// BSP v30 (GoldSrc map, see assets/bsp.cpp)
// ---------------------------------------------------------------------------

constexpr int kLumpEntities = 0;
constexpr int kLumpPlanes = 1;
constexpr int kLumpTextures = 2;
constexpr int kLumpVertexes = 3;
constexpr int kLumpTexInfo = 6;
constexpr int kLumpFaces = 7;
constexpr int kLumpClipNodes = 9;
constexpr int kLumpEdges = 12;
constexpr int kLumpSurfEdges = 13;
constexpr int kLumpModels = 14;
constexpr int kLumpCount = 15;

#pragma pack(push, 1)
struct BspLump {
    int32_t fileOfs;
    int32_t fileLen;
};
struct BspHeaderRaw {
    int32_t version;
    BspLump lumps[kLumpCount];
};
struct BspVec3 {
    float x, y, z;
};
struct BspDPlane {
    float normal[3];
    float dist;
    int32_t type;
};
struct BspDClipNode {
    int32_t planeNum;
    int16_t children[2];
};
struct BspDModel {
    float mins[3], maxs[3];
    float origin[3];
    int32_t headNode[4];
    int32_t visLeafs;
    int32_t firstFace, numFaces;
};
#pragma pack(pop)

// Builds a minimal but structurally valid BSP v30 file: an entity lump (raw
// text) plus one clip plane / clipnode pair carving space in half along X,
// and one model whose hull-1 head node points at that clipnode. Everything
// else (textures, faces, vertices, edges) is left empty, which the loader
// tolerates.
struct BspBuilder {
    std::string entityText;
    std::vector<BspDPlane> planes;
    std::vector<BspDClipNode> clipNodes;
    std::vector<BspDModel> models;

    std::string build() const {
        std::vector<uint8_t> entityData(entityText.begin(), entityText.end());
        std::vector<uint8_t> planeData;
        for (auto& p : planes) append(planeData, p);
        std::vector<uint8_t> clipData;
        for (auto& c : clipNodes) append(clipData, c);
        std::vector<uint8_t> modelData;
        for (auto& m : models) append(modelData, m);

        std::vector<std::pair<int, const std::vector<uint8_t>*>> lumpData = {
            {kLumpEntities, &entityData},
            {kLumpPlanes, &planeData},
            {kLumpClipNodes, &clipData},
            {kLumpModels, &modelData},
        };

        BspHeaderRaw header{};
        header.version = 30;
        std::vector<uint8_t> body;
        int32_t cursor = (int32_t)sizeof(BspHeaderRaw);
        // All lumps default to empty (offset arbitrary, length 0); fill in the
        // ones we actually populate.
        for (auto& lump : header.lumps) lump = BspLump{cursor, 0};
        for (auto& [index, data] : lumpData) {
            header.lumps[index].fileOfs = cursor;
            header.lumps[index].fileLen = (int32_t)data->size();
            body.insert(body.end(), data->begin(), data->end());
            cursor += (int32_t)data->size();
        }

        std::vector<uint8_t> file;
        append(file, header);
        file.insert(file.end(), body.begin(), body.end());

        std::string path = tempFilePath(".bsp");
        writeFile(path, file);
        return path;
    }
};

// ---------------------------------------------------------------------------
// Studio MDL v10 (GoldSrc character model, see assets/mdl.cpp)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct MdlStudioHeader {
    int32_t ident;
    int32_t version;
    char name[64];
    int32_t length;

    float eyePosition[3];
    float minB[3], maxB[3];
    float bbMin[3], bbMax[3];

    int32_t flags;

    int32_t numBones, boneIndex;
    int32_t numBoneControllers, boneControllerIndex;
    int32_t numHitboxes, hitboxIndex;
    int32_t numSeq, seqIndex;
    int32_t numSeqGroups, seqGroupIndex;
    int32_t numTextures, textureIndex, textureDataIndex;
    int32_t numSkinRef, numSkinFamilies, skinIndex;
    int32_t numBodyParts, bodyPartIndex;
    int32_t numAttachments, attachmentIndex;
    int32_t soundTable, soundIndex, soundGroups, soundGroupIndex;
    int32_t numTransitions, transitionIndex;
};
struct MdlStudioBone {
    char name[32];
    int32_t parent;
    int32_t flags;
    int32_t boneController[6];
    float value[6];
    float scale[6];
};
struct MdlStudioTexture {
    char name[64];
    int32_t flags;
    int32_t width, height;
    int32_t index;
};
struct MdlStudioBodyPart {
    char name[64];
    int32_t numModels;
    int32_t base;
    int32_t modelIndex;
};
struct MdlStudioModel {
    char name[64];
    int32_t type;
    float boundingRadius;
    int32_t numMesh, meshIndex;
    int32_t numVerts, vertInfoIndex, vertIndex;
    int32_t numNorms, normInfoIndex, normIndex;
    int32_t numGroups, groupIndex;
};
struct MdlStudioMesh {
    int32_t numTris;
    int32_t triIndex;
    int32_t skinRef;
    int32_t numNorms;
    int32_t normIndex;
};
#pragma pack(pop)

// Builds a minimal single-triangle, single-bone, single-texture MDL v10 file
// (a "fan" command of 3 verts = 1 triangle) at the origin bone pose. Returns
// the file path.
inline std::string buildSimpleMdl() {
    // One 2x2 solid-color texture, no palette transparency.
    const uint32_t texW = 2, texH = 2;
    std::vector<uint8_t> texPixels(texW * texH, 1); // all pixel index 1
    uint8_t palette[256 * 3] = {0};
    palette[1 * 3 + 0] = 200;
    palette[1 * 3 + 1] = 100;
    palette[1 * 3 + 2] = 50;

    // Bone: identity pose at origin, no parent.
    MdlStudioBone bone{};
    setName(bone.name, sizeof(bone.name), "root");
    bone.parent = -1;

    // Triangle vertices, all bound to bone 0.
    float verts[9] = {
        0, 0, 0,
        10, 0, 0,
        0, 10, 0,
    };
    uint8_t vertBoneIndex[3] = {0, 0, 0};

    // Triangle command list for one mesh: a 3-vert fan (positive count),
    // terminated by a 0 count. Each vert is 4 int16: vertIndex, normIndex, s, t.
    int16_t cmds[] = {
        3, 0, 0, 0, 0,
            1, 0, 0, 0,
            2, 0, 0, 0,
        0, // terminator
    };

    MdlStudioMesh mesh{};
    mesh.numTris = 1;
    mesh.skinRef = 0;

    MdlStudioModel model{};
    setName(model.name, sizeof(model.name), "body");
    model.numMesh = 1;
    model.numVerts = 3;

    MdlStudioBodyPart bodyPart{};
    setName(bodyPart.name, sizeof(bodyPart.name), "studio");
    bodyPart.numModels = 1;
    bodyPart.base = 0;

    MdlStudioTexture tex{};
    setName(tex.name, sizeof(tex.name), "skin");
    tex.width = (int32_t)texW;
    tex.height = (int32_t)texH;

    int16_t skinRefs[1] = {0};

    // Lay out the variable-length sections back-to-back after the fixed
    // header, tracking each section's byte offset from the start of the file.
    MdlStudioHeader hdr{};
    hdr.ident = 0x54534449; // "IDST" little-endian
    hdr.version = 10;
    setName(hdr.name, sizeof(hdr.name), "test.mdl");
    hdr.numBones = 1;
    hdr.numTextures = 1;
    hdr.numSkinRef = 1;
    hdr.numSkinFamilies = 1;
    hdr.numBodyParts = 1;

    int32_t offset = sizeof(MdlStudioHeader);
    hdr.boneIndex = offset; offset += sizeof(MdlStudioBone) * hdr.numBones;
    hdr.skinIndex = offset; offset += sizeof(int16_t) * hdr.numSkinRef;
    hdr.textureIndex = offset; offset += sizeof(MdlStudioTexture) * hdr.numTextures;
    hdr.bodyPartIndex = offset; offset += sizeof(MdlStudioBodyPart) * hdr.numBodyParts;
    int32_t modelOffset = offset; offset += sizeof(MdlStudioModel);
    int32_t vertInfoOffset = offset; offset += 3; // 3 bytes, vertBoneIndex
    int32_t vertOffset = offset; offset += sizeof(verts);
    int32_t meshOffset = offset; offset += sizeof(MdlStudioMesh);
    int32_t triOffset = offset; offset += sizeof(cmds);
    hdr.textureDataIndex = offset;
    int32_t texDataOffset = offset; offset += texPixels.size() + sizeof(palette);

    bodyPart.modelIndex = modelOffset;
    model.vertInfoIndex = vertInfoOffset;
    model.vertIndex = vertOffset;
    model.meshIndex = meshOffset;
    mesh.triIndex = triOffset;
    tex.index = texDataOffset;

    hdr.length = offset;

    std::vector<uint8_t> file(offset, 0);
    auto put = [&](int32_t at, const void* data, size_t n) {
        std::memcpy(file.data() + at, data, n);
    };
    put(0, &hdr, sizeof(hdr));
    put(hdr.boneIndex, &bone, sizeof(bone));
    put(hdr.skinIndex, skinRefs, sizeof(skinRefs));
    put(hdr.textureIndex, &tex, sizeof(tex));
    put(hdr.bodyPartIndex, &bodyPart, sizeof(bodyPart));
    put(modelOffset, &model, sizeof(model));
    put(vertInfoOffset, vertBoneIndex, sizeof(vertBoneIndex));
    put(vertOffset, verts, sizeof(verts));
    put(meshOffset, &mesh, sizeof(mesh));
    put(triOffset, cmds, sizeof(cmds));
    put(texDataOffset, texPixels.data(), texPixels.size());
    put(texDataOffset + (int32_t)texPixels.size(), palette, sizeof(palette));

    std::string path = tempFilePath(".mdl");
    writeFile(path, file);
    return path;
}

} // namespace fixtures
