#include "test_framework.h"
#include "fixtures.h"

#include "assets/mdl.h"

TEST(mdl_open_missing_file_fails) {
    MdlModel model;
    CHECK(!model.load("/nonexistent/path/does_not_exist.mdl"));
}

TEST(mdl_open_rejects_bad_ident) {
    std::vector<uint8_t> bogus(sizeof(fixtures::MdlStudioHeader), 0);
    std::string path = fixtures::tempFilePath(".mdl");
    fixtures::writeFile(path, bogus);

    MdlModel model;
    CHECK(!model.load(path));
}

TEST(mdl_loads_single_triangle) {
    std::string path = fixtures::buildSimpleMdl();

    MdlModel model;
    CHECK(model.load(path));
    CHECK_EQ(model.triangles().size(), (size_t)1);
    CHECK_EQ(model.textures().size(), (size_t)1);
    // Guard the indexing so a size mismatch reports a failed check instead of
    // crashing the whole suite on an out-of-range access.
    if (model.triangles().empty() || model.textures().empty()) return;

    const MdlTriangle& tri = model.triangles()[0];
    CHECK_NEAR(tri.a.x, 0.0, 1e-4);
    CHECK_NEAR(tri.a.y, 0.0, 1e-4);
    CHECK_NEAR(tri.b.x, 10.0, 1e-4);
    CHECK_NEAR(tri.c.y, 10.0, 1e-4);
    CHECK_EQ(tri.textureIndex, 0);

    const MdlTexture& tex = model.textures()[0];
    CHECK_EQ(tex.width, (uint32_t)2);
    CHECK_EQ(tex.height, (uint32_t)2);
    CHECK_EQ(tex.rgba.size(), (size_t)2 * 2 * 4);
    if (tex.rgba.size() < 4) return;
    CHECK_EQ(tex.rgba[0], (uint8_t)200);
    CHECK_EQ(tex.rgba[1], (uint8_t)100);
    CHECK_EQ(tex.rgba[2], (uint8_t)50);
    CHECK_EQ(tex.rgba[3], (uint8_t)255);
}

// Regression: every offset in a studio header was dereferenced without a range
// check. A header claiming sections far outside the file must be rejected
// cleanly rather than reading out of bounds.
TEST(mdl_survives_offsets_pointing_past_end_of_file) {
    fixtures::MdlStudioHeader hdr{};
    hdr.ident = 0x54534449; // "IDST"
    hdr.version = 10;
    hdr.numBones = 1000;
    hdr.boneIndex = 500000;
    hdr.numTextures = 1000;
    hdr.textureIndex = 500000;
    hdr.numBodyParts = 1000;
    hdr.bodyPartIndex = 500000;
    hdr.numSkinRef = 1000;
    hdr.skinIndex = 500000;

    std::vector<uint8_t> file(sizeof(hdr));
    std::memcpy(file.data(), &hdr, sizeof(hdr));
    std::string path = fixtures::tempFilePath(".mdl");
    fixtures::writeFile(path, file);

    MdlModel model;
    model.load(path); // must return, not read past the buffer
    CHECK_EQ(model.triangles().size(), (size_t)0);
    CHECK_EQ(model.textures().size(), (size_t)0);
}

// Regression: negative offsets would index backwards out of the buffer.
TEST(mdl_rejects_negative_offsets) {
    fixtures::MdlStudioHeader hdr{};
    hdr.ident = 0x54534449;
    hdr.version = 10;
    hdr.numBones = 4;
    hdr.boneIndex = -2048;
    hdr.numTextures = 4;
    hdr.textureIndex = -2048;
    hdr.numBodyParts = 4;
    hdr.bodyPartIndex = -2048;

    std::vector<uint8_t> file(sizeof(hdr));
    std::memcpy(file.data(), &hdr, sizeof(hdr));
    std::string path = fixtures::tempFilePath(".mdl");
    fixtures::writeFile(path, file);

    MdlModel model;
    model.load(path);
    CHECK_EQ(model.triangles().size(), (size_t)0);
}

// Regression: a vertex count large enough that numVerts * 3 overflows int32
// back to a small positive number would pass a naive range check while the
// copy loop still walked the full (huge) count.
TEST(mdl_rejects_vertex_count_that_overflows_when_scaled) {
    fixtures::MdlStudioHeader hdr{};
    hdr.ident = 0x54534449;
    hdr.version = 10;
    hdr.numBones = 1;
    hdr.numBodyParts = 1;

    auto align4 = [](int32_t v) { return (v + 3) & ~3; };
    int32_t offset = (int32_t)sizeof(hdr);
    hdr.boneIndex = offset; offset = align4(offset + (int32_t)sizeof(fixtures::MdlStudioBone));
    hdr.bodyPartIndex = offset; offset = align4(offset + (int32_t)sizeof(fixtures::MdlStudioBodyPart));
    int32_t modelOffset = offset; offset = align4(offset + (int32_t)sizeof(fixtures::MdlStudioModel));

    fixtures::MdlStudioBone bone{};
    bone.parent = -1;
    fixtures::MdlStudioBodyPart part{};
    part.numModels = 1;
    part.modelIndex = modelOffset;
    fixtures::MdlStudioModel model{};
    model.numVerts = 0x55555556; // * 3 wraps to 2 in int32
    model.vertIndex = (int32_t)sizeof(hdr);
    model.vertInfoIndex = (int32_t)sizeof(hdr);

    std::vector<uint8_t> file(offset + 1024, 0);
    std::memcpy(file.data(), &hdr, sizeof(hdr));
    std::memcpy(file.data() + hdr.boneIndex, &bone, sizeof(bone));
    std::memcpy(file.data() + hdr.bodyPartIndex, &part, sizeof(part));
    std::memcpy(file.data() + modelOffset, &model, sizeof(model));

    std::string path = fixtures::tempFilePath(".mdl");
    fixtures::writeFile(path, file);

    MdlModel m;
    m.load(path); // must bail out instead of walking 1.4 billion vertices
    CHECK_EQ(m.triangles().size(), (size_t)0);
}

int main() { return RUN_ALL_TESTS(); }
