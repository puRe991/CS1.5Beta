#include "test_framework.h"
#include "fixtures.h"

#include "assets/bsp.h"

TEST(bsp_open_missing_file_fails) {
    BspMap map;
    CHECK(!map.load("/nonexistent/path/does_not_exist.bsp", {}));
}

TEST(bsp_open_rejects_wrong_version) {
    fixtures::BspHeaderRaw header{};
    header.version = 29; // only 30 is supported
    std::vector<uint8_t> file(sizeof(header));
    std::memcpy(file.data(), &header, sizeof(header));
    std::string path = fixtures::tempFilePath(".bsp");
    fixtures::writeFile(path, file);

    BspMap map;
    CHECK(!map.load(path, {}));
}

TEST(bsp_parses_entity_lump) {
    fixtures::BspBuilder builder;
    builder.entityText =
        "{\n\"classname\" \"worldspawn\"\n\"wad\" \"cstrike/foo.wad\"\n}\n"
        "{\n\"classname\" \"info_player_start\"\n\"origin\" \"10 20 30\"\n}\n";
    std::string path = builder.build();

    BspMap map;
    CHECK(map.load(path, {}));
    CHECK_EQ(map.entities().size(), (size_t)2);

    const std::string* classname = map.entities()[1].get("classname");
    CHECK(classname != nullptr);
    if (classname) CHECK_EQ(*classname, std::string("info_player_start"));

    CHECK(map.entities()[0].get("missing_key") == nullptr);
}

TEST(bsp_model_index_for_parses_star_prefixed_model_key) {
    BspEntity ent;
    ent.pairs.emplace_back("model", "*16");
    CHECK_EQ(BspMap::modelIndexFor(ent), 16);

    BspEntity noModel;
    CHECK_EQ(BspMap::modelIndexFor(noModel), -1);

    BspEntity badModel;
    badModel.pairs.emplace_back("model", "models/foo.mdl"); // brush entities use *N, not this
    CHECK_EQ(BspMap::modelIndexFor(badModel), -1);
}

// Builds a map split in half by the plane x=0 (normal +X), with clip node 0
// as hull 1's head node: side >= 0 (x >= 0) goes to CONTENTS_EMPTY (-1), the
// other side to CONTENTS_SOLID (-2). Contents values are always negative in
// this format — a non-negative child is another node index, so using 0 here
// would self-reference node 0 and infinite-loop pointInSolid.
TEST(bsp_point_in_solid_respects_clip_plane) {
    fixtures::BspBuilder builder;
    fixtures::BspDPlane plane{};
    plane.normal[0] = 1;
    plane.normal[1] = 0;
    plane.normal[2] = 0;
    plane.dist = 0;
    builder.planes.push_back(plane);

    fixtures::BspDClipNode node{};
    node.planeNum = 0;
    node.children[0] = -1; // CONTENTS_EMPTY on the +X side
    node.children[1] = -2; // CONTENTS_SOLID on the -X side
    builder.clipNodes.push_back(node);

    fixtures::BspDModel model{};
    model.headNode[1] = 0; // hull 1 head node = our single clip node
    builder.models.push_back(model);

    std::string path = builder.build();
    BspMap map;
    CHECK(map.load(path, {}));

    CHECK(!map.pointInSolid(Vec3{5, 0, 0}));  // +X side: empty
    CHECK(map.pointInSolid(Vec3{-5, 0, 0}));  // -X side: solid
}

TEST(bsp_trace_line_finds_hit_on_solid_side) {
    fixtures::BspBuilder builder;
    fixtures::BspDPlane plane{};
    plane.normal[0] = 1;
    plane.dist = 0;
    builder.planes.push_back(plane);

    fixtures::BspDClipNode node{};
    node.planeNum = 0;
    node.children[0] = -1; // CONTENTS_EMPTY
    node.children[1] = -2; // CONTENTS_SOLID
    builder.clipNodes.push_back(node);

    fixtures::BspDModel model{};
    model.headNode[1] = 0;
    builder.models.push_back(model);

    std::string path = builder.build();
    BspMap map;
    CHECK(map.load(path, {}));

    Vec3 hit{};
    // Traces from deep in solid (-50) toward empty (+50): should report a hit
    // at the very start since it's already inside solid geometry.
    CHECK(map.traceLine(Vec3{-50, 0, 0}, Vec3{50, 0, 0}, hit));
    CHECK(hit.x <= 0.0f);

    // Entirely in empty space: no hit.
    CHECK(!map.traceLine(Vec3{10, 0, 0}, Vec3{50, 0, 0}, hit));
}

int main() { return RUN_ALL_TESTS(); }
