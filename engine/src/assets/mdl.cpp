#include "mdl.h"
#include "limits.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

struct StudioHeader {
    int32_t ident;
    int32_t version;
    char name[64];
    int32_t length;

    float eyePosition[3];
    float min[3], max[3];
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

struct StudioBone {
    char name[32];
    int32_t parent;
    int32_t flags;
    int32_t boneController[6];
    float value[6];
    float scale[6];
};

struct StudioTexture {
    char name[64];
    int32_t flags;
    int32_t width, height;
    int32_t index;
};

struct StudioBodyPart {
    char name[64];
    int32_t numModels;
    int32_t base;
    int32_t modelIndex;
};

struct StudioModel {
    char name[64];
    int32_t type;
    float boundingRadius;
    int32_t numMesh, meshIndex;
    int32_t numVerts, vertInfoIndex, vertIndex;
    int32_t numNorms, normInfoIndex, normIndex;
    int32_t numGroups, groupIndex;
};

struct StudioMesh {
    int32_t numTris;
    int32_t triIndex;
    int32_t skinRef;
    int32_t numNorms;
    int32_t normIndex;
};

constexpr int32_t kStudioNfMasked = 0x40;

// 3x3 rotation + translation, composed as world = parent * local.
struct BoneXform {
    float r[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
    float t[3] = {0, 0, 0};
};

BoneXform makeLocal(const float value[6]) {
    float roll = value[3], pitch = value[4], yaw = value[5];
    float sr = std::sin(roll), cr = std::cos(roll);
    float sp = std::sin(pitch), cp = std::cos(pitch);
    float sy = std::sin(yaw), cy = std::cos(yaw);

    float rx[3][3] = {{1,0,0}, {0,cr,-sr}, {0,sr,cr}};
    float ry[3][3] = {{cp,0,sp}, {0,1,0}, {-sp,0,cp}};
    float rz[3][3] = {{cy,-sy,0}, {sy,cy,0}, {0,0,1}};

    float ryx[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            ryx[i][j] = ry[i][0]*rx[0][j] + ry[i][1]*rx[1][j] + ry[i][2]*rx[2][j];

    BoneXform out;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            out.r[i][j] = rz[i][0]*ryx[0][j] + rz[i][1]*ryx[1][j] + rz[i][2]*ryx[2][j];
    out.t[0] = value[0];
    out.t[1] = value[1];
    out.t[2] = value[2];
    return out;
}

BoneXform compose(const BoneXform& parent, const BoneXform& local) {
    BoneXform out;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out.r[i][j] = parent.r[i][0]*local.r[0][j] + parent.r[i][1]*local.r[1][j] + parent.r[i][2]*local.r[2][j];
        }
        out.t[i] = parent.r[i][0]*local.t[0] + parent.r[i][1]*local.t[1] + parent.r[i][2]*local.t[2] + parent.t[i];
    }
    return out;
}

void apply(const BoneXform& x, float vx, float vy, float vz, float& ox, float& oy, float& oz) {
    ox = x.r[0][0]*vx + x.r[0][1]*vy + x.r[0][2]*vz + x.t[0];
    oy = x.r[1][0]*vx + x.r[1][1]*vy + x.r[1][2]*vz + x.t[1];
    oz = x.r[2][0]*vx + x.r[2][1]*vy + x.r[2][2]*vz + x.t[2];
}

// Every offset/count pair in a studio header comes from the file, so each one
// is validated before it is turned into a pointer. Returns nullptr when the
// requested span doesn't fit inside the loaded data.
template <typename T>
const T* spanAt(const std::vector<uint8_t>& data, int32_t offset, int32_t count) {
    if (offset < 0 || count <= 0) return nullptr;
    size_t start = (size_t)offset;
    if (start > data.size()) return nullptr;
    // Division instead of multiplication, so a huge count can't overflow.
    if ((size_t)count > (data.size() - start) / sizeof(T)) return nullptr;
    // The struct is read straight out of the buffer, so a misaligned offset
    // would be undefined behaviour. Real models are always aligned; anything
    // else is malformed and gets rejected rather than read.
    if ((reinterpret_cast<uintptr_t>(data.data()) + start) % alignof(T) != 0) return nullptr;
    return reinterpret_cast<const T*>(data.data() + start);
}

} // namespace

bool MdlModel::load(const std::string& path) {
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

    if (data.size() < sizeof(StudioHeader)) return false;
    const StudioHeader* hdr = reinterpret_cast<const StudioHeader*>(data.data());
    if (std::memcmp(&hdr->ident, "IDST", 4) != 0 || hdr->version != 10) return false;

    // --- Bones: compute a world-space bind-pose transform for each ---
    const StudioBone* bones = spanAt<StudioBone>(data, hdr->boneIndex, hdr->numBones);
    std::vector<BoneXform> boneWorld(bones ? hdr->numBones : 0);
    for (int32_t i = 0; bones && i < hdr->numBones; ++i) {
        BoneXform local = makeLocal(bones[i].value);
        // A parent must already be resolved, i.e. appear earlier in the list;
        // anything else (forward or self reference) is treated as rootless.
        bool validParent = bones[i].parent >= 0 && bones[i].parent < i;
        boneWorld[i] = validParent ? compose(boneWorld[bones[i].parent], local) : local;
    }

    // --- Textures ---
    textures_.clear();
    const StudioTexture* studioTex = spanAt<StudioTexture>(data, hdr->textureIndex, hdr->numTextures);
    for (int32_t i = 0; studioTex && i < hdr->numTextures; ++i) {
        const StudioTexture& st = studioTex[i];
        MdlTexture tex;
        tex.name.assign(st.name, strnlen(st.name, sizeof(st.name)));
        tex.width = (uint32_t)st.width;
        tex.height = (uint32_t)st.height;

        size_t pixelCount = (size_t)st.width * st.height;
        bool sane = st.width > 0 && st.height > 0 &&
                    (uint32_t)st.width <= kMaxTextureDim && (uint32_t)st.height <= kMaxTextureDim;
        if (sane && spanAt<uint8_t>(data, st.index, (int32_t)(pixelCount + 256 * 3))) {
            const uint8_t* pixels = data.data() + st.index;
            const uint8_t* palette = pixels + pixelCount;
            bool masked = (st.flags & kStudioNfMasked) != 0;

            tex.rgba.resize(pixelCount * 4);
            for (size_t p = 0; p < pixelCount; ++p) {
                uint8_t idx = pixels[p];
                tex.rgba[p*4+0] = palette[idx*3+0];
                tex.rgba[p*4+1] = palette[idx*3+1];
                tex.rgba[p*4+2] = palette[idx*3+2];
                tex.rgba[p*4+3] = (masked && idx == 255) ? 0 : 255;
            }
        }
        textures_.push_back(std::move(tex));
    }

    // Skin family 0: skinref -> texture index.
    const int16_t* skinRefs = spanAt<int16_t>(data, hdr->skinIndex, hdr->numSkinRef);

    // --- Body parts: geometry, using submodel 0 of each (default variant) ---
    triangles_.clear();
    const StudioBodyPart* bodyParts = spanAt<StudioBodyPart>(data, hdr->bodyPartIndex, hdr->numBodyParts);
    const int16_t* cmdsEnd = reinterpret_cast<const int16_t*>(data.data() + data.size());

    for (int32_t bp = 0; bodyParts && bp < hdr->numBodyParts; ++bp) {
        const StudioBodyPart& part = bodyParts[bp];
        if (part.numModels <= 0) continue;
        const StudioModel* model = spanAt<StudioModel>(data, part.modelIndex, 1);
        if (!model) continue;

        // Bound numVerts before it is multiplied: numVerts * 3 on a wild value
        // could overflow int32 back into a small positive count, which would
        // pass the span check while the loop below still ran the full count.
        const int32_t numVerts = model->numVerts;
        if (numVerts <= 0 || (size_t)numVerts > data.size() / sizeof(float) / 3) continue;

        const float* verts = spanAt<float>(data, model->vertIndex, numVerts * 3);
        const uint8_t* vertBoneIndex = spanAt<uint8_t>(data, model->vertInfoIndex, numVerts);
        if (!verts || !vertBoneIndex || boneWorld.empty()) continue;
        std::vector<float> worldVerts((size_t)numVerts * 3);
        for (int32_t v = 0; v < numVerts; ++v) {
            uint8_t boneIdx = vertBoneIndex[v];
            if (boneIdx >= boneWorld.size()) boneIdx = 0;
            apply(boneWorld[boneIdx], verts[v*3+0], verts[v*3+1], verts[v*3+2],
                  worldVerts[v*3+0], worldVerts[v*3+1], worldVerts[v*3+2]);
        }

        const StudioMesh* meshes = spanAt<StudioMesh>(data, model->meshIndex, model->numMesh);
        for (int32_t m = 0; meshes && m < model->numMesh; ++m) {
            const StudioMesh& mesh = meshes[m];
            int textureIndex = (skinRefs && mesh.skinRef >= 0 && mesh.skinRef < hdr->numSkinRef)
                                   ? skinRefs[mesh.skinRef] : -1;

            const int16_t* cmds = spanAt<int16_t>(data, mesh.triIndex, 1);
            // The command stream is length-prefixed per run and terminated by a
            // zero count, with no total size in the header — so the end of the
            // file is the only hard bound, and every read is checked against it.
            while (cmds && cmds < cmdsEnd) {
                int16_t count = *cmds++;
                if (count == 0) break;

                bool isStrip = count > 0;
                int n = count > 0 ? count : -count;
                if ((ptrdiff_t)n * 4 > cmdsEnd - cmds) break; // 4 int16 per vertex

                std::vector<MdlVertex> runVerts;
                runVerts.reserve(n);
                for (int i = 0; i < n; ++i) {
                    int16_t vi = cmds[0];
                    // cmds[1] is the normal index, unused (unlit rendering for now).
                    int16_t s = cmds[2];
                    int16_t t = cmds[3];
                    cmds += 4;

                    MdlVertex mv{};
                    if (vi >= 0 && vi < numVerts) {
                        mv.x = worldVerts[vi*3+0];
                        mv.y = worldVerts[vi*3+1];
                        mv.z = worldVerts[vi*3+2];
                    }
                    mv.u = (float)s;
                    mv.v = (float)t;
                    runVerts.push_back(mv);
                }

                for (int i = 2; i < n; ++i) {
                    MdlTriangle tri;
                    if (isStrip) {
                        if (i % 2 == 0) { tri.a = runVerts[i-2]; tri.b = runVerts[i-1]; tri.c = runVerts[i]; }
                        else            { tri.a = runVerts[i-1]; tri.b = runVerts[i-2]; tri.c = runVerts[i]; }
                    } else {
                        tri.a = runVerts[0]; tri.b = runVerts[i-1]; tri.c = runVerts[i];
                    }
                    tri.textureIndex = textureIndex;
                    triangles_.push_back(tri);
                }
            }
        }
        // Only submodel 0 of each bodypart is used (the default variant).
    }

    return true;
}
