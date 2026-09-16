#include "mdl.h"
#include "limits.h"

#include <algorithm>
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

// Matches the HL SDK's mstudioattachment_t layout exactly (32+4+4+12+36=88
// bytes): name, an unused type field, the owning bone, and the attachment's
// local-space origin relative to that bone (vectors[3] are basis vectors
// for orientation — unused here, we only need the position).
struct StudioAttachment {
    char name[32];
    int32_t type;
    int32_t bone;
    float org[3];
    float vectors[3][3];
};

// Matches mstudiobbox_t (32 bytes): the bone it rides on, a hit-group id
// (the HL SDK's HITGROUP_* constants — 0 generic, 1 head, 2 chest, 3
// stomach, 4/5 left/right arm, 6/7 left/right leg — the actual, canonical
// way the original engine assigns body regions, not a name guess), and a
// bone-local axis-aligned box.
struct StudioBBox {
    int32_t bone;
    int32_t group;
    float bbMin[3];
    float bbMax[3];
};

// Matches mstudioseqdesc_t (176 bytes) — only the fields playback needs are
// named; the rest (events, pivots, per-sequence bounding box, transition
// graph) are skipped over via padding since nothing here uses them yet.
struct StudioSeqDesc {
    char label[32];
    float fps;
    int32_t flags; // bit 0: STUDIO_LOOPING
    int32_t activity;
    int32_t actWeight;
    int32_t numEvents, eventIndex;
    int32_t numFrames;
    int32_t numPivots, pivotIndex;
    int32_t motionType, motionBone;
    float linearMovement[3];
    int32_t automoveposIndex, automoveangleIndex;
    float bbMin[3], bbMax[3];
    int32_t numBlends;
    int32_t animIndex; // offset (from the sequence group's data base) to numBlends*numBones StudioAnim entries
    int32_t blendType[2];
    float blendStart[2], blendEnd[2];
    int32_t blendParent;
    int32_t seqGroup; // which mstudioseqgroup_t this sequence's data lives in — only 0 (embedded) is supported
    int32_t entryNode, exitNode, nodeFlags;
    int32_t nextSeq;
};

constexpr int32_t kStudioLooping = 0x0001;

// mstudioanim_t: for one bone, a byte offset (relative to this struct's own
// address) into a run of StudioAnimValue for each of 6 channels (X,Y,Z
// position, then X,Y,Z rotation) — 0 means "not animated, use the bone's
// static bind-pose value for this channel".
struct StudioAnim {
    uint16_t offset[6];
};

// RLE-compressed animation keyframe stream, decoded by extractAnimValue().
union StudioAnimValue {
    struct {
        uint8_t valid; // how many raw values immediately follow this header
        uint8_t total; // how many frames this run (valid values + held last value) covers
    } num;
    int16_t value;
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

// Standard GoldSrc animation-value decode (see Half-Life SDK's
// Studio_GetAnimValue): each channel is a run-length-encoded stream of
// (valid, total) headers followed by `valid` raw int16 values — "total"
// frames are covered per header, with any frames beyond `valid` holding
// the last decoded value rather than storing it again.
int16_t extractAnimValue(const StudioAnimValue* panimvalue, int frame) {
    int k = frame;
    while (panimvalue->num.total <= k) {
        k -= panimvalue->num.total;
        panimvalue += panimvalue->num.valid + 1;
        if (panimvalue->num.total == 0) return 0; // past the end of the stream
    }
    if (panimvalue->num.valid > k) {
        return panimvalue[k + 1].value;
    }
    return panimvalue[panimvalue->num.valid].value;
}

// One bone's animated value[6] (position xyz, rotation xyz) at a fractional
// frame, falling back to the bind-pose value for any channel this sequence
// doesn't animate.
void extractBoneFrame(const StudioBone& bone, const StudioAnim& anim, float frame, float outValue[6]) {
    int frameA = (int)frame;
    int frameB = frameA + 1;
    float t = frame - (float)frameA;
    for (int c = 0; c < 6; ++c) {
        if (anim.offset[c] == 0) {
            outValue[c] = bone.value[c];
            continue;
        }
        const StudioAnimValue* stream = reinterpret_cast<const StudioAnimValue*>(
            reinterpret_cast<const uint8_t*>(&anim) + anim.offset[c]);
        float a = (float)extractAnimValue(stream, frameA);
        float b = (float)extractAnimValue(stream, frameB);
        outValue[c] = bone.value[c] + (a + (b - a) * t) * bone.scale[c];
    }
}

// Shared by both the static bind pose (load()) and animated playback
// (MdlModel::pose()) — turns a set of world-space bone transforms into the
// flat triangle soup, re-skinning every vertex against its owning bone.
std::vector<MdlTriangle> buildTriangles(const StudioHeader* hdr, const std::vector<uint8_t>& data,
                                         const std::vector<BoneXform>& boneWorld,
                                         const std::vector<MdlTexture>& textures) {
    std::vector<MdlTriangle> triangles;
    const int16_t* skinRefs = spanAt<int16_t>(data, hdr->skinIndex, hdr->numSkinRef);
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
                    triangles.push_back(tri);
                }
            }
        }
        // Only submodel 0 of each bodypart is used (the default variant).
    }
    (void)textures; // texture dimensions aren't needed here (u/v stay in texel units)
    return triangles;
}

// Bind-pose world transform for every bone: each bone's static value[6]
// composed with its (already-resolved) parent. Shared by load() (for the
// bind-pose triangles/attachments/hitboxes) and pose()/poseHitboxes() as
// the fallback when no animated sequence applies.
std::vector<BoneXform> computeBindBoneWorld(const StudioHeader* hdr, const StudioBone* bones) {
    std::vector<BoneXform> boneWorld(bones ? hdr->numBones : 0);
    for (int32_t i = 0; bones && i < hdr->numBones; ++i) {
        BoneXform local = makeLocal(bones[i].value);
        bool validParent = bones[i].parent >= 0 && bones[i].parent < i;
        boneWorld[i] = validParent ? compose(boneWorld[bones[i].parent], local) : local;
    }
    return boneWorld;
}

// Animated world transform for every bone at a fractional frame within one
// sequence, decoding that sequence's compressed per-bone tracks the same
// way pose() does. Returns an empty vector if the sequence can't be played
// (out of range, or its data lives in an unsupported external seqgroup).
std::vector<BoneXform> computeAnimatedBoneWorld(const StudioHeader* hdr, const std::vector<uint8_t>& data,
                                                 const StudioBone* bones, const StudioSeqDesc& seq,
                                                 float frame, float blend) {
    if (seq.seqGroup != 0) return {};
    frame = std::clamp(frame, 0.0f, (float)(seq.numFrames - 1));
    int32_t numBlends = std::max(1, seq.numBlends);
    if (hdr->numBones <= 0 || numBlends > INT32_MAX / hdr->numBones) return {};
    const StudioAnim* anims = spanAt<StudioAnim>(data, seq.animIndex, numBlends * hdr->numBones);
    if (!anims) return {};

    float blendPos = std::clamp(blend, 0.0f, 1.0f) * (float)(numBlends - 1);
    int32_t blendA = (int32_t)blendPos;
    int32_t blendB = std::min(blendA + 1, numBlends - 1);
    float blendT = blendPos - (float)blendA;

    std::vector<BoneXform> boneWorld(hdr->numBones);
    for (int32_t i = 0; i < hdr->numBones; ++i) {
        float valueA[6], valueB[6];
        extractBoneFrame(bones[i], anims[(size_t)blendA * hdr->numBones + i], frame, valueA);
        float value[6];
        if (blendA == blendB) {
            std::memcpy(value, valueA, sizeof(value));
        } else {
            extractBoneFrame(bones[i], anims[(size_t)blendB * hdr->numBones + i], frame, valueB);
            for (int c = 0; c < 6; ++c) value[c] = valueA[c] + (valueB[c] - valueA[c]) * blendT;
        }
        BoneXform local = makeLocal(value);
        bool validParent = bones[i].parent >= 0 && bones[i].parent < i;
        boneWorld[i] = validParent ? compose(boneWorld[bones[i].parent], local) : local;
    }
    return boneWorld;
}

// The HL SDK's HITGROUP_* constants (public, engine-defined ids — not
// copyrightable text), the actual mapping the original engine uses to turn
// a hitbox's raw group number into a named body region.
BodyPart bodyPartForGroupImpl(int32_t group) {
    switch (group) {
        case 1: return BodyPart::Head;
        case 2: return BodyPart::Chest;
        case 3: return BodyPart::Stomach;
        case 4: return BodyPart::LeftArm;
        case 5: return BodyPart::RightArm;
        case 6: return BodyPart::LeftLeg;
        case 7: return BodyPart::RightLeg;
        default: return BodyPart::Generic;
    }
}

} // namespace

BodyPart bodyPartForHitGroup(int group) { return bodyPartForGroupImpl(group); }

const char* bodyPartName(BodyPart part) {
    switch (part) {
        case BodyPart::Head:     return "Head";
        case BodyPart::Chest:    return "Chest";
        case BodyPart::Stomach:  return "Stomach";
        case BodyPart::LeftArm:  return "Left Arm";
        case BodyPart::RightArm: return "Right Arm";
        case BodyPart::LeftLeg:  return "Left Leg";
        case BodyPart::RightLeg: return "Right Leg";
        default:                 return "Generic";
    }
}

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
    std::vector<BoneXform> boneWorld = computeBindBoneWorld(hdr, bones);

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

    // --- Attachment points: muzzle origin, weapon-to-hand bone, etc. ---
    attachments_.clear();
    const StudioAttachment* studioAttach = spanAt<StudioAttachment>(data, hdr->attachmentIndex, hdr->numAttachments);
    for (int32_t i = 0; studioAttach && i < hdr->numAttachments; ++i) {
        const StudioAttachment& sa = studioAttach[i];
        MdlAttachment att;
        att.name.assign(sa.name, strnlen(sa.name, sizeof(sa.name)));
        int32_t boneIdx = sa.bone;
        if (boneIdx < 0 || (size_t)boneIdx >= boneWorld.size()) boneIdx = 0;
        if (!boneWorld.empty()) {
            apply(boneWorld[boneIdx], sa.org[0], sa.org[1], sa.org[2], att.x, att.y, att.z);
        }
        attachments_.push_back(std::move(att));
    }

    // --- Hitboxes: per-bone hit-group boxes used for damage detection.
    // Stored in bone-local space here; poseHitboxes() transforms them into
    // world space at whatever pose is currently active. ---
    hitboxes_.clear();
    const StudioBBox* studioBoxes = spanAt<StudioBBox>(data, hdr->hitboxIndex, hdr->numHitboxes);
    for (int32_t i = 0; studioBoxes && i < hdr->numHitboxes; ++i) {
        const StudioBBox& sb = studioBoxes[i];
        MdlHitbox hb;
        hb.bone = sb.bone;
        hb.group = sb.group;
        hb.part = bodyPartForHitGroup(sb.group);
        hb.mins[0] = sb.bbMin[0]; hb.mins[1] = sb.bbMin[1]; hb.mins[2] = sb.bbMin[2];
        hb.maxs[0] = sb.bbMax[0]; hb.maxs[1] = sb.bbMax[1]; hb.maxs[2] = sb.bbMax[2];
        hitboxes_.push_back(hb);
    }

    // --- Sequences: name/fps/frame count for playback, actual per-bone
    // animation data is decoded on demand in pose() ---
    sequences_.clear();
    const StudioSeqDesc* seqDescs = spanAt<StudioSeqDesc>(data, hdr->seqIndex, hdr->numSeq);
    for (int32_t i = 0; seqDescs && i < hdr->numSeq; ++i) {
        const StudioSeqDesc& sd = seqDescs[i];
        MdlSequence seq;
        seq.name.assign(sd.label, strnlen(sd.label, sizeof(sd.label)));
        seq.fps = sd.fps > 0.0f ? sd.fps : 30.0f;
        seq.numFrames = std::max(1, sd.numFrames);
        seq.looping = (sd.flags & kStudioLooping) != 0;
        seq.numBlends = std::max(1, sd.numBlends);
        sequences_.push_back(std::move(seq));
    }

    triangles_ = buildTriangles(hdr, data, boneWorld, textures_);
    fileData_ = std::move(data);
    return true;
}

const MdlAttachment* MdlModel::findAttachment(const std::string& name) const {
    for (const auto& a : attachments_) {
        if (a.name == name) return &a;
    }
    return nullptr;
}

int MdlModel::findSequence(const std::string& name) const {
    for (size_t i = 0; i < sequences_.size(); ++i) {
        if (sequences_[i].name == name) return (int)i;
    }
    return -1;
}

std::vector<MdlTriangle> MdlModel::pose(int sequenceIndex, float frame, float blend) const {
    if (sequenceIndex < 0 || (size_t)sequenceIndex >= sequences_.size() || fileData_.empty()) {
        return triangles_;
    }

    const std::vector<uint8_t>& data = fileData_;
    if (data.size() < sizeof(StudioHeader)) return triangles_;
    const StudioHeader* hdr = reinterpret_cast<const StudioHeader*>(data.data());

    const StudioBone* bones = spanAt<StudioBone>(data, hdr->boneIndex, hdr->numBones);
    const StudioSeqDesc* seqDescs = spanAt<StudioSeqDesc>(data, hdr->seqIndex, hdr->numSeq);
    if (!bones || !seqDescs || sequenceIndex >= hdr->numSeq) return triangles_;

    // Only sequences embedded in the main file (seqgroup 0, no external
    // demand-loaded animation blob) are supported — true of every sequence
    // in the CS1.5 asset set this was tested against.
    std::vector<BoneXform> boneWorld = computeAnimatedBoneWorld(hdr, data, bones, seqDescs[sequenceIndex], frame, blend);
    if (boneWorld.empty()) return triangles_;

    return buildTriangles(hdr, data, boneWorld, textures_);
}

std::vector<WorldHitbox> MdlModel::poseHitboxes(int sequenceIndex, float frame, float blend) const {
    std::vector<WorldHitbox> out;
    if (hitboxes_.empty() || fileData_.empty()) return out;

    const std::vector<uint8_t>& data = fileData_;
    if (data.size() < sizeof(StudioHeader)) return out;
    const StudioHeader* hdr = reinterpret_cast<const StudioHeader*>(data.data());
    const StudioBone* bones = spanAt<StudioBone>(data, hdr->boneIndex, hdr->numBones);
    if (!bones) return out;

    std::vector<BoneXform> boneWorld;
    if (sequenceIndex >= 0) {
        const StudioSeqDesc* seqDescs = spanAt<StudioSeqDesc>(data, hdr->seqIndex, hdr->numSeq);
        if (seqDescs && sequenceIndex < hdr->numSeq) {
            boneWorld = computeAnimatedBoneWorld(hdr, data, bones, seqDescs[sequenceIndex], frame, blend);
        }
    }
    if (boneWorld.empty()) boneWorld = computeBindBoneWorld(hdr, bones); // no sequence, or one that failed to decode

    for (const MdlHitbox& hb : hitboxes_) {
        if (hb.bone < 0 || (size_t)hb.bone >= boneWorld.size()) continue;
        const BoneXform& x = boneWorld[hb.bone];

        // A rotated box's 8 corners transformed individually, re-enclosed
        // into an axis-aligned world box — conservative (can be a little
        // larger than the exact rotated volume) but simple and correct for
        // a hit test, same tradeoff the original engine's own AABB-only
        // hit detection makes.
        float wmin[3] = {1e30f, 1e30f, 1e30f};
        float wmax[3] = {-1e30f, -1e30f, -1e30f};
        for (int c = 0; c < 8; ++c) {
            float local[3] = {
                (c & 1) ? hb.maxs[0] : hb.mins[0],
                (c & 2) ? hb.maxs[1] : hb.mins[1],
                (c & 4) ? hb.maxs[2] : hb.mins[2],
            };
            float wx, wy, wz;
            apply(x, local[0], local[1], local[2], wx, wy, wz);
            wmin[0] = std::min(wmin[0], wx); wmax[0] = std::max(wmax[0], wx);
            wmin[1] = std::min(wmin[1], wy); wmax[1] = std::max(wmax[1], wy);
            wmin[2] = std::min(wmin[2], wz); wmax[2] = std::max(wmax[2], wz);
        }

        WorldHitbox whb;
        whb.bone = hb.bone;
        whb.part = hb.part;
        whb.mins[0] = wmin[0]; whb.mins[1] = wmin[1]; whb.mins[2] = wmin[2];
        whb.maxs[0] = wmax[0]; whb.maxs[1] = wmax[1]; whb.maxs[2] = wmax[2];
        out.push_back(whb);
    }
    return out;
}
