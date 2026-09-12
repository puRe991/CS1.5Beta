#include "bsp.h"
#include "wad.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {

constexpr int kLumpEntities = 0;
constexpr int kLumpPlanes = 1;
constexpr int kLumpTextures = 2;
constexpr int kLumpVertexes = 3;
constexpr int kLumpVisibility = 4;
constexpr int kLumpNodes = 5;
constexpr int kLumpTexInfo = 6;
constexpr int kLumpFaces = 7;
constexpr int kLumpLighting = 8;
constexpr int kLumpClipNodes = 9;
constexpr int kLumpLeafs = 10;
constexpr int kLumpMarkSurfaces = 11;
constexpr int kLumpEdges = 12;
constexpr int kLumpSurfEdges = 13;
constexpr int kLumpModels = 14;
constexpr int kLumpCount = 15;

constexpr int32_t kContentsSolid = -2;

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

struct DPlane {
    float normal[3];
    float dist;
    int32_t type;
};

struct DClipNode {
    int32_t planeNum;
    int16_t children[2];
};

struct DModel {
    float mins[3], maxs[3];
    float origin[3];
    int32_t headNode[4]; // one BSP tree per hull: 0=point, 1=player box, 2=large box, 3=crouch box
    int32_t visLeafs;
    int32_t firstFace, numFaces;
};

struct DNode {
    int32_t planeNum;
    int16_t children[2]; // negative = -(leaf index)-1
    int16_t mins[3], maxs[3];
    uint16_t firstFace, numFaces;
};

struct DLeaf {
    int32_t contents;
    int32_t visOfs; // -1 = no vis data
    int16_t mins[3], maxs[3];
    uint16_t firstMarkSurface, numMarkSurfaces;
    uint8_t ambientLevels[4];
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
    std::vector<uint8_t> planeData, clipNodeData, modelData, lightData;
    std::vector<uint8_t> nodeData, leafData, markSurfaceData;
    bool ok = readLump(f, header.lumps[kLumpEntities], entityData) &&
              readLump(f, header.lumps[kLumpTextures], texData) &&
              readLump(f, header.lumps[kLumpVertexes], vertexData) &&
              readLump(f, header.lumps[kLumpTexInfo], texInfoData) &&
              readLump(f, header.lumps[kLumpFaces], faceData) &&
              readLump(f, header.lumps[kLumpEdges], edgeData) &&
              readLump(f, header.lumps[kLumpSurfEdges], surfEdgeData) &&
              readLump(f, header.lumps[kLumpPlanes], planeData) &&
              readLump(f, header.lumps[kLumpClipNodes], clipNodeData) &&
              readLump(f, header.lumps[kLumpModels], modelData) &&
              readLump(f, header.lumps[kLumpLighting], lightData) &&
              readLump(f, header.lumps[kLumpNodes], nodeData) &&
              readLump(f, header.lumps[kLumpLeafs], leafData) &&
              readLump(f, header.lumps[kLumpMarkSurfaces], markSurfaceData) &&
              readLump(f, header.lumps[kLumpVisibility], visData_);
    std::fclose(f);
    if (!ok) return false;

    nodes_.clear();
    for (size_t o = 0; o + sizeof(DNode) <= nodeData.size(); o += sizeof(DNode)) {
        const DNode* n = reinterpret_cast<const DNode*>(nodeData.data() + o);
        nodes_.push_back({n->planeNum, {n->children[0], n->children[1]}});
    }

    leafs_.clear();
    for (size_t o = 0; o + sizeof(DLeaf) <= leafData.size(); o += sizeof(DLeaf)) {
        const DLeaf* l = reinterpret_cast<const DLeaf*>(leafData.data() + o);
        leafs_.push_back({l->visOfs, l->firstMarkSurface, l->numMarkSurfaces});
    }

    markSurfaces_.clear();
    for (size_t o = 0; o + sizeof(uint16_t) <= markSurfaceData.size(); o += sizeof(uint16_t)) {
        markSurfaces_.push_back(*reinterpret_cast<const uint16_t*>(markSurfaceData.data() + o));
    }

    planes_.clear();
    for (size_t o = 0; o + sizeof(DPlane) <= planeData.size(); o += sizeof(DPlane)) {
        const DPlane* p = reinterpret_cast<const DPlane*>(planeData.data() + o);
        planes_.push_back({p->normal[0], p->normal[1], p->normal[2], p->dist});
    }

    clipNodes_.clear();
    for (size_t o = 0; o + sizeof(DClipNode) <= clipNodeData.size(); o += sizeof(DClipNode)) {
        const DClipNode* c = reinterpret_cast<const DClipNode*>(clipNodeData.data() + o);
        clipNodes_.push_back({c->planeNum, {c->children[0], c->children[1]}});
    }

    hull1HeadNode_ = -1;
    models_.clear();
    for (size_t o = 0; o + sizeof(DModel) <= modelData.size(); o += sizeof(DModel)) {
        const DModel* m = reinterpret_cast<const DModel*>(modelData.data() + o);
        models_.push_back(BspModelBounds{
            Vec3{m->mins[0], m->mins[1], m->mins[2]},
            Vec3{m->maxs[0], m->maxs[1], m->maxs[2]}
        });
    }
    renderHeadNode_ = -1;
    if (!models_.empty()) {
        hull1HeadNode_ = reinterpret_cast<const DModel*>(modelData.data())->headNode[1];
        renderHeadNode_ = reinterpret_cast<const DModel*>(modelData.data())->headNode[0];
    }

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

    // "black"/"white" are plain solid-color utility textures GoldSrc maps
    // use throughout (shadow-catcher brushes, trim, etc.) — safe to
    // synthesize directly rather than needing them to come from a WAD,
    // since there's nothing stylized to reproduce.
    for (auto& t : textures_) {
        if (!t.rgba.empty()) continue;
        std::string lower = toLower(t.name);
        if (lower != "black" && lower != "white") continue;
        uint8_t channel = lower == "black" ? 0 : 255;
        t.rgba.assign((size_t)t.width * t.height * 4, channel);
        for (size_t p = 3; p < t.rgba.size(); p += 4) t.rgba[p] = 255; // alpha always opaque
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
    rawToCompactFace_.assign(numFaces, -1);

    for (size_t fi = 0; fi < numFaces; ++fi) {
        const DFace& df = faces[fi];
        if (df.texInfo < 0 || (size_t)df.texInfo >= numTexInfos) continue;
        const TexInfo& ti = texInfos[df.texInfo];
        if (ti.miptexIndex < 0 || (size_t)ti.miptexIndex >= textures_.size()) continue;

        // Tool textures the original engine never actually renders (they
        // exist purely for the compiler: trigger volumes, player-clip
        // brushes, rotation origins, and the sky brush — which this engine
        // draws separately via Skybox). Rendering them as ordinary faces
        // would just show as "missing texture" noise for content that was
        // never meant to be visible in the first place.
        static const char* kNonRenderedTextures[] = {"aaatrigger", "clip", "origin", "sky", "null"};
        std::string texName = toLower(textures_[ti.miptexIndex].name);
        bool nonRendered = false;
        for (const char* skip : kNonRenderedTextures) {
            if (texName == skip) { nonRendered = true; break; }
        }
        if (nonRendered) continue;

        BspFace face;
        face.textureIndex = ti.miptexIndex;

        float lmMinU = 1e30f, lmMinV = 1e30f, lmMaxU = -1e30f, lmMaxV = -1e30f;
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

            lmMinU = std::min(lmMinU, u); lmMaxU = std::max(lmMaxU, u);
            lmMinV = std::min(lmMinV, vcoord); lmMaxV = std::max(lmMaxV, vcoord);
        }

        // Lightmap: GoldSrc bakes one luxel per 16 world units. Luxel-space
        // texcoords are the same u/v used for the base texture, just
        // rebased to this face's own min corner and divided by 16.
        if (!face.texCoords.empty()) {
            constexpr float kLuxelSize = 16.0f;
            int lmMinCellU = (int)std::floor(lmMinU / kLuxelSize);
            int lmMinCellV = (int)std::floor(lmMinV / kLuxelSize);
            int lmMaxCellU = (int)std::floor(lmMaxU / kLuxelSize);
            int lmMaxCellV = (int)std::floor(lmMaxV / kLuxelSize);
            face.lightmapWidth = (uint32_t)(lmMaxCellU - lmMinCellU + 1);
            face.lightmapHeight = (uint32_t)(lmMaxCellV - lmMinCellV + 1);

            for (size_t i = 0; i < face.texCoords.size(); i += 2) {
                face.lightmapTexCoords.push_back(face.texCoords[i] / kLuxelSize - lmMinCellU);
                face.lightmapTexCoords.push_back(face.texCoords[i + 1] / kLuxelSize - lmMinCellV);
            }

            size_t lightmapBytes = (size_t)face.lightmapWidth * face.lightmapHeight * 3;
            if (df.lightOfs >= 0 && df.styles[0] != 255 &&
                (size_t)df.lightOfs + lightmapBytes <= lightData.size()) {
                face.lightmapRGB.assign(lightData.begin() + df.lightOfs, lightData.begin() + df.lightOfs + lightmapBytes);
            }
        }

        if (face.vertices.size() >= 3) {
            rawToCompactFace_[fi] = (int32_t)faces_.size();
            faces_.push_back(std::move(face));
        }
    }

    // --- Per-face leaf ownership, for locality-sorting PVS draw batches ---
    faceLeaf_.assign(faces_.size(), -1);
    for (size_t li = 0; li < leafs_.size(); ++li) {
        const Leaf& leaf = leafs_[li];
        for (uint32_t m = 0; m < leaf.numMarkSurfaces; ++m) {
            size_t entry = (size_t)leaf.firstMarkSurface + m;
            if (entry >= markSurfaces_.size()) continue;
            uint16_t raw = markSurfaces_[entry];
            if (raw >= rawToCompactFace_.size()) continue;
            int32_t compact = rawToCompactFace_[raw];
            if (compact >= 0 && faceLeaf_[compact] < 0) faceLeaf_[compact] = (int32_t)li;
        }
    }

    (void)numEdges;
    (void)numSurfEdges;
    return true;
}

int32_t BspMap::findLeaf(Vec3 point) const {
    if (renderHeadNode_ < 0 || nodes_.empty()) return -1;
    int32_t node = renderHeadNode_;
    while (node >= 0) {
        const RenderNode& n = nodes_[node];
        if (n.planeNum < 0 || (size_t)n.planeNum >= planes_.size()) return -1;
        const Plane& pl = planes_[n.planeNum];
        float d = pl.nx * point.x + pl.ny * point.y + pl.nz * point.z - pl.dist;
        node = d >= 0 ? n.children[0] : n.children[1];
    }
    return -node - 1;
}

std::vector<bool> BspMap::computeVisibleFaces(Vec3 viewPos) const {
    std::vector<bool> result; // empty = fallback to "draw everything"
    if (leafs_.empty() || visData_.empty() || markSurfaces_.empty()) return result;

    int32_t leafIndex = findLeaf(viewPos);
    // Leaf 0 is always the shared "outside the world" / solid leaf and
    // carries no PVS row — nothing meaningful to cull against, so bail out
    // to the safe default rather than culling everything.
    if (leafIndex <= 0 || (size_t)leafIndex >= leafs_.size()) return result;

    const Leaf& viewLeaf = leafs_[leafIndex];
    if (viewLeaf.visOfs < 0 || (size_t)viewLeaf.visOfs >= visData_.size()) return result;

    // Decompress the RLE-encoded PVS row: one bit per leaf, excluding leaf
    // 0 (bit i corresponds to leaf i+1). A zero byte means "N more zero
    // bytes follow" (the run length is the next byte); anything else is a
    // literal byte of bits.
    size_t numLeafBytes = (leafs_.size() + 7) / 8;
    std::vector<uint8_t> decompressed(numLeafBytes, 0);
    size_t bytePos = 0;
    size_t srcPos = (size_t)viewLeaf.visOfs;
    while (bytePos < numLeafBytes && srcPos < visData_.size()) {
        if (visData_[srcPos] == 0) {
            if (srcPos + 1 >= visData_.size()) break;
            bytePos += visData_[srcPos + 1]; // already zero-initialized
            srcPos += 2;
        } else {
            decompressed[bytePos] = visData_[srcPos];
            ++bytePos;
            ++srcPos;
        }
    }

    result.assign(faces_.size(), false);
    auto markLeafFaces = [&](int32_t li) {
        if (li <= 0 || (size_t)li >= leafs_.size()) return;
        const Leaf& leaf = leafs_[li];
        for (uint32_t m = 0; m < leaf.numMarkSurfaces; ++m) {
            size_t entry = (size_t)leaf.firstMarkSurface + m;
            if (entry >= markSurfaces_.size()) continue;
            uint16_t raw = markSurfaces_[entry];
            if (raw >= rawToCompactFace_.size()) continue;
            int32_t compact = rawToCompactFace_[raw];
            if (compact >= 0) result[compact] = true;
        }
    };
    markLeafFaces(leafIndex); // always include the leaf the viewer is standing in
    for (size_t li = 1; li < leafs_.size(); ++li) {
        size_t bit = li - 1;
        if ((decompressed[bit / 8] >> (bit % 8)) & 1) markLeafFaces((int32_t)li);
    }
    return result;
}

int BspMap::modelIndexFor(const BspEntity& ent) {
    const std::string* model = ent.get("model");
    if (!model || model->empty() || (*model)[0] != '*') return -1;
    return std::atoi(model->c_str() + 1);
}

bool BspMap::pointInSolid(Vec3 point) const {
    Vec3 unused;
    return pointInSolid(point, unused);
}

bool BspMap::pointInSolid(Vec3 point, Vec3& outPlaneNormal) const {
    if (hull1HeadNode_ < 0 || clipNodes_.empty()) return false;

    int32_t node = hull1HeadNode_;
    outPlaneNormal = Vec3{0, 0, 1};
    while (node >= 0) {
        const ClipNode& cn = clipNodes_[node];
        if (cn.planeNum < 0 || (size_t)cn.planeNum >= planes_.size()) return false;
        const Plane& pl = planes_[cn.planeNum];
        float d = pl.nx * point.x + pl.ny * point.y + pl.nz * point.z - pl.dist;
        // Orient so the normal always points away from the side this step
        // is about to descend into (the side that ultimately turns out to
        // be solid) — the true outward surface normal, not just the
        // plane's stored (arbitrary) direction.
        float sign = d >= 0 ? -1.0f : 1.0f;
        outPlaneNormal = Vec3{pl.nx * sign, pl.ny * sign, pl.nz * sign};
        node = d >= 0 ? cn.children[0] : cn.children[1];
    }
    return node == kContentsSolid;
}

bool BspMap::traceLine(Vec3 start, Vec3 end, Vec3& outHit, Vec3* outNormal) const {
    constexpr float kStep = 4.0f;
    float dx = end.x - start.x, dy = end.y - start.y, dz = end.z - start.z;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-4f) return false;
    dx /= len; dy /= len; dz /= len;

    for (float t = 0.0f; t <= len; t += kStep) {
        Vec3 p{start.x + dx * t, start.y + dy * t, start.z + dz * t};
        Vec3 planeNormal;
        if (pointInSolid(p, planeNormal)) {
            outHit = p;
            if (outNormal) *outNormal = planeNormal;
            return true;
        }
    }
    return false;
}
