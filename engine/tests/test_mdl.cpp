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
    CHECK_EQ(tex.rgba[0], (uint8_t)200);
    CHECK_EQ(tex.rgba[1], (uint8_t)100);
    CHECK_EQ(tex.rgba[2], (uint8_t)50);
    CHECK_EQ(tex.rgba[3], (uint8_t)255);
}

int main() { return RUN_ALL_TESTS(); }
