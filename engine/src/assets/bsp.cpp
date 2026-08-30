#include "bsp.h"
#include "wad.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>

namespace {

constexpr int kLumpEntities = 0;
constexpr int kLumpTextures = 2;
constexpr int kLumpVertexes = 3;
constexpr int kLumpTexInfo = 6;
constexpr int kLumpFaces = 7;
constexpr int kLumpEdges = 12;
constexpr int kLumpSurfEdges = 13;
constexpr int kLumpCount = 15;

struct Lump {
    int32_t fileOfs;
    int32_t fileLen;
};

struct Header {
    int32_t version;
    Lump lumps[kLumpCount];
};

struct DEdge {
    uint16_t v[2];
};

struct DFace {
    uint16_t planeNum;
    int16_t side;
    int32_t firstEdge;
    int16_t numEdges;
    int16_t texInfo;
    uint8_t styles[4];
    int32_t lightOfs;
};

struct TexInfo {
    float vecs[2][4];
    int32_t miptexIndex;
    int32_t flags;
};

struct MipTexHeader {
    char name[16];
    uint32_t width;
    uint32_t height;
    uint32_t offsets[4];
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool readLump(FILE* f, const Lump& lump, std::vector<uint8_t>& out) {
    out.resize(lump.fileLen);
    if (lump.fileLen == 0) return true;
    std::fseek(f, lump.fileOfs, SEEK_SET);
    return std::fread(out.data(), 1, lump.fileLen, f) == (size_t)lump.fileLen;
}

} // namespace

const std::string* BspEntity::get(const std::string& key) const {
    for (const auto& kv : pairs) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

void BspMap::parseEntities(const std::string& text) {
    entities_.clear();
    size_t pos = 0;
    while (true) {
        size_t open = text.find('{', pos);
        if (open == std::string::npos) break;
        size_t close = text.find('}', open);
        if (close == std::string::npos) break;

        BspEntity ent;
        size_t i = open + 1;
        while (true) {
            size_t k0 = text.find('"', i);
            if (k0 == std::string::npos || k0 > close) break;
            size_t k1 = text.find('"', k0 + 1);
            if (k1 == std::string::npos || k1 > close) break;
            size_t v0 = text.find('"', k1 + 1);
            if (v0 == std::string::npos || v0 > close) break;
            size_t v1 = text.find('"', v0 + 1);
            if (v1 == std::string::npos || v1 > close) break;

            std::string key = text.substr(k0 + 1, k1 - k0 - 1);
            std::string value = text.substr(v0 + 1, v1 - v0 - 1);
            ent.pairs.emplace_back(std::move(key), std::move(value));
            i = v1 + 1;
        }
        entities_.push_back(std::move(ent));
        pos = close + 1;
    }
}

bool BspMap::load(const std::string& path, const std::vector<std::string>& externalWadDirs) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    Header header;
    if (std::fread(&header, sizeof(header), 1, f) != 1 || header.version != 30) {
        std::fclose(f);
        return false;
    }

    std::vector<uint8_t> entityData, texData, vertexData, texInfoData, faceData, edgeData, surfEdgeData;
    bool ok = readLump(f, header.lumps[kLumpEntities], entityData) &&
              readLump(f, header.lumps[kLumpTextures], texData) &&
              readLump(f, header.lumps[kLumpVertexes], vertexData) &&
              readLump(f, header.lumps[kLumpTexInfo], texInfoData) &&
              readLump(f, header.lumps[kLumpFaces], faceData) &&
              readLump(f, header.lumps[kLumpEdges], edgeData) &&
              readLump(f, header.lumps[kLumpSurfEdges], surfEdgeData);
    std::fclose(f);
    if (!ok) return false;

    parseEntities(std::string(entityData.begin(), entityData.end()));

    const Vec3* vertices = reinterpret_cast<const Vec3*>(vertexData.data());
    size_t numVertices = vertexData.size() / sizeof(Vec3);
    const DEdge* edges = reinterpret_cast<const DEdge*>(edgeData.data());
    size_t numEdges = edgeData.size() / sizeof(DEdge);
    const int32_t* surfEdges = reinterpret_cast<const int32_t*>(surfEdgeData.data());
    size_t numSurfEdges = surfEdgeData.size() / sizeof(int32_t);
    const TexInfo* texInfos = reinterpret_cast<const TexInfo*>(texInfoData.data());
    size_t numTexInfos = texInfoData.size() / sizeof(TexInfo);
    const DFace* faces = reinterpret_cast<const DFace*>(faceData.data());
    size_t numFaces = faceData.size() / sizeof(DFace);

    // --- Textures: embedded miptex lump, decoding external refs from WADs on demand ---
    textures_.clear();
    std::map<std::string, int> nameToIndex;

    std::vector<WadFile> externalWads;
    if (texData.size() >= 4) {
        int32_t numMipTex = *reinterpret_cast<const int32_t*>(texData.data());
        const int32_t* offsets = reinterpret_cast<const int32_t*>(texData.data() + 4);

        for (int32_t i = 0; i < numMipTex; ++i) {
            if ((size_t)(4 + (i + 1) * 4) > texData.size()) break;
            int32_t ofs = offsets[i];
            if (ofs < 0 || (size_t)(ofs + (int32_t)sizeof(MipTexHeader)) > texData.size()) continue;

            const MipTexHeader* mip = reinterpret_cast<const MipTexHeader*>(texData.data() + ofs);
            std::string name(mip->name, strnlen(mip->name, sizeof(mip->name)));

            BspTexture tex;
            tex.name = name;
            tex.width = mip->width;
            tex.height = mip->height;

            if (mip->offsets[0] != 0) {
                // Embedded pixel data + palette, same layout as a WAD3 miptex lump.
                const uint8_t* base = texData.data() + ofs;
                size_t indexCount = (size_t)mip->width * mip->height;
                const uint8_t* indices = base + mip->offsets[0];
                size_t mip3Size = (mip->width / 8) * (mip->height / 8);
                const uint8_t* palette = base + mip->offsets[3] + mip3Size + 2;

                if (ofs + (int32_t)(mip->offsets[3] + mip3Size + 2 + 256 * 3) <= (int32_t)texData.size()) {
                    tex.rgba.resize(indexCount * 4);
                    for (size_t p = 0; p < indexCount; ++p) {
                        uint8_t idx = indices[p];
                        tex.rgba[p * 4 + 0] = palette[idx * 3 + 0];
                        tex.rgba[p * 4 + 1] = palette[idx * 3 + 1];
                        tex.rgba[p * 4 + 2] = palette[idx * 3 + 2];
                        tex.rgba[p * 4 + 3] = 255;
                    }
                }
            }

            nameToIndex[toLower(name)] = (int)textures_.size();
            textures_.push_back(std::move(tex));
        }
    }

    // Resolve textures still missing pixel data from external WAD files referenced
    // by worldspawn's "wad" key (semicolon-separated paths from the original build).
    bool anyUnresolved = false;
    for (auto& t : textures_) {
        if (t.rgba.empty()) anyUnresolved = true;
    }
    if (anyUnresolved && !entities_.empty()) {
        const std::string* wadKey = entities_[0].get("wad");
        std::vector<std::string> wadBaseNames;
        if (wadKey) {
            size_t pos = 0;
            while (pos < wadKey->size()) {
                size_t sep = wadKey->find(';', pos);
                std::string entry = wadKey->substr(pos, sep == std::string::npos ? std::string::npos : sep - pos);
                size_t slash = entry.find_last_of("/\\");
                std::string base = slash == std::string::npos ? entry : entry.substr(slash + 1);
                if (!base.empty()) wadBaseNames.push_back(base);
                if (sep == std::string::npos) break;
                pos = sep + 1;
            }
        }

        for (const auto& dir : externalWadDirs) {
            for (const auto& base : wadBaseNames) {
                WadFile wad;
                if (wad.open(dir + "/" + base)) {
                    for (size_t i = 0; i < wad.textureCount(); ++i) {
                        auto it = nameToIndex.find(toLower(wad.textureName(i)));
                        if (it == nameToIndex.end() || !textures_[it->second].rgba.empty()) continue;
                        WadTexture decoded;
                        if (wad.decodeTexture(i, decoded)) {
                            textures_[it->second].rgba = std::move(decoded.rgba);
                            textures_[it->second].width = decoded.width;
                            textures_[it->second].height = decoded.height;
                        }
                    }
                }
            }
        }
    }

    // --- Faces: build a vertex fan per face from surfedges, with UVs from texinfo ---
    faces_.clear();
    faces_.reserve(numFaces);

    for (size_t fi = 0; fi < numFaces; ++fi) {
        const DFace& df = faces[fi];
        if (df.texInfo < 0 || (size_t)df.texInfo >= numTexInfos) continue;
        const TexInfo& ti = texInfos[df.texInfo];
        if (ti.miptexIndex < 0 || (size_t)ti.miptexIndex >= textures_.size()) continue;

        BspFace face;
        face.textureIndex = ti.miptexIndex;

        for (int16_t e = 0; e < df.numEdges; ++e) {
            int32_t se = surfEdges[df.firstEdge + e];
            uint16_t vi = se >= 0 ? edges[se].v[0] : edges[-se].v[1];
            if (vi >= numVertices) continue;
            Vec3 v = vertices[vi];
            face.vertices.push_back(v);

            float u = v.x * ti.vecs[0][0] + v.y * ti.vecs[0][1] + v.z * ti.vecs[0][2] + ti.vecs[0][3];
            float vcoord = v.x * ti.vecs[1][0] + v.y * ti.vecs[1][1] + v.z * ti.vecs[1][2] + ti.vecs[1][3];
            face.texCoords.push_back(u);
            face.texCoords.push_back(vcoord);
        }

        if (face.vertices.size() >= 3) faces_.push_back(std::move(face));
    }

    (void)numEdges;
    (void)numSurfEdges;
    return true;
}
