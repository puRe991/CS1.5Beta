#include "test_framework.h"
#include "fixtures.h"

#include "nav.h"

namespace {
// A wide-open map: no clipnodes at all, so every straight line is
// unobstructed — isolates the graph/pathfinding logic itself from any
// geometry concerns (those are covered by the floor-plane test below).
std::string buildOpenMap() {
    fixtures::BspBuilder builder;
    builder.entityText = "{\n\"classname\" \"worldspawn\"\n}\n";
    return builder.build();
}

// Two floor quads sharing an edge at x=100 (each 100x100, well above the
// graph's minimum polygon area) — real navmesh geometry, not clip-tree
// collision (nav polygon nodes come straight from BspFace data and never
// touch pointInSolidHull), so no clipnodes are needed for this fixture.
std::string buildTwoAdjacentFloorQuads() {
    fixtures::BspBuilder builder;
    builder.entityText = "{\n\"classname\" \"worldspawn\"\n}\n";
    builder.textureCount = 1;

    fixtures::BspTexInfo texInfo{};
    texInfo.miptexIndex = 0;
    texInfo.vecs[0][0] = 1.0f;
    texInfo.vecs[1][1] = 1.0f;
    builder.texInfos.push_back(texInfo);

    builder.vertices = {
        {0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0},     // quad A
        {100, 0, 0}, {200, 0, 0}, {200, 100, 0}, {100, 100, 0}, // quad B, shares A's x=100 edge
    };
    builder.edges = {
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
    };
    builder.surfEdges = {0, 1, 2, 3, 4, 5, 6, 7};

    fixtures::BspDFace faceA{};
    faceA.firstEdge = 0; faceA.numEdges = 4; faceA.texInfo = 0;
    fixtures::BspDFace faceB{};
    faceB.firstEdge = 4; faceB.numEdges = 4; faceB.texInfo = 0;
    builder.faces.push_back(faceA);
    builder.faces.push_back(faceB);

    return builder.build();
}

// A floor quad (kept), a wall quad (same size, but vertical — filtered by
// the upward-normal check), and a tiny floor quad (filtered by the minimum
// area check).
std::string buildFloorWallAndTinyFaces() {
    fixtures::BspBuilder builder;
    builder.entityText = "{\n\"classname\" \"worldspawn\"\n}\n";
    builder.textureCount = 1;

    fixtures::BspTexInfo texInfo{};
    texInfo.miptexIndex = 0;
    texInfo.vecs[0][0] = 1.0f;
    texInfo.vecs[1][1] = 1.0f;
    builder.texInfos.push_back(texInfo);

    builder.vertices = {
        {0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0},         // A: real floor, 100x100
        {0, 0, 0}, {0, 100, 0}, {0, 100, 100}, {0, 0, 100},         // B: vertical wall, 100x100
        {500, 500, 0}, {510, 500, 0}, {510, 510, 0}, {500, 510, 0}, // C: floor, but only 10x10
    };
    builder.edges = {
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
        {{8, 9}}, {{9, 10}}, {{10, 11}}, {{11, 8}},
    };
    builder.surfEdges = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

    fixtures::BspDFace faceA{}; faceA.firstEdge = 0; faceA.numEdges = 4; faceA.texInfo = 0;
    fixtures::BspDFace faceB{}; faceB.firstEdge = 4; faceB.numEdges = 4; faceB.texInfo = 0;
    fixtures::BspDFace faceC{}; faceC.firstEdge = 8; faceC.numEdges = 4; faceC.texInfo = 0;
    builder.faces.push_back(faceA);
    builder.faces.push_back(faceB);
    builder.faces.push_back(faceC);

    return builder.build();
}

// A single infinite horizontal plane at z=0 (solid below, empty above) —
// enough geometry for the floor-probe/grid-sampling logic to actually find
// a floor, without needing a full boxed-in room.
std::string buildFlatFloorMap() {
    fixtures::BspBuilder builder;
    builder.entityText = "{\n\"classname\" \"worldspawn\"\n}\n";
    fixtures::BspDPlane plane{};
    plane.normal[0] = 0; plane.normal[1] = 0; plane.normal[2] = 1;
    plane.dist = 0;
    builder.planes.push_back(plane);

    fixtures::BspDClipNode node{};
    node.planeNum = 0;
    node.children[0] = -1; // z >= 0: empty (air)
    node.children[1] = -2; // z < 0: solid (ground)
    builder.clipNodes.push_back(node);

    fixtures::BspDModel model{};
    model.headNode[1] = 0;
    builder.models.push_back(model);
    return builder.build();
}
} // namespace

TEST(nav_graph_connects_seeds_within_edge_distance) {
    std::string path = buildOpenMap();
    BspMap map;
    CHECK(map.load(path, {}));

    // 0 and 2 are 2000 apart (beyond the graph's max edge distance), but
    // each is 1000 from the middle seed — a path must exist and it must
    // route through node 1, not jump straight there.
    std::vector<Vec3> seeds = {Vec3{0, 0, 0}, Vec3{1000, 0, 0}, Vec3{2000, 0, 0}};
    NavGraph graph;
    // Tiny map bounds well away from the seeds, so no grid sampling adds
    // extra nodes/edges that could confuse what's being tested here.
    graph.build(map, seeds, Vec3{-1, -1, -1}, Vec3{1, 1, 1});

    CHECK(!graph.empty());
    std::vector<Vec3> route = graph.findPath(Vec3{0, 0, 0}, Vec3{2000, 0, 0});
    CHECK(route.size() >= 2); // at least the middle waypoint plus the goal
    if (!route.empty()) {
        Vec3 last = route.back();
        CHECK_NEAR(last.x, 2000.0, 1.0);
    }
    bool passesThroughMiddle = false;
    for (const Vec3& p : route) {
        if (std::abs(p.x - 1000.0f) < 1.0f) passesThroughMiddle = true;
    }
    CHECK(passesThroughMiddle);
}

TEST(nav_graph_find_path_empty_when_unreachable) {
    std::string path = buildOpenMap();
    BspMap map;
    CHECK(map.load(path, {}));

    // Two seeds far enough apart that no edge connects them, and no grid
    // sampling in between (degenerate map bounds) to bridge the gap.
    std::vector<Vec3> seeds = {Vec3{0, 0, 0}, Vec3{5000, 0, 0}};
    NavGraph graph;
    graph.build(map, seeds, Vec3{-1, -1, -1}, Vec3{1, 1, 1});

    std::vector<Vec3> route = graph.findPath(Vec3{0, 0, 0}, Vec3{5000, 0, 0});
    CHECK(route.empty());
}

TEST(nav_graph_find_path_on_empty_graph_is_empty) {
    NavGraph graph; // never built
    CHECK(graph.empty());
    CHECK(graph.findPath(Vec3{0, 0, 0}, Vec3{10, 10, 10}).empty());
    CHECK_EQ(graph.nearestNode(Vec3{0, 0, 0}), -1);
}

TEST(nav_graph_samples_floor_grid_at_the_right_height) {
    std::string path = buildFlatFloorMap();
    BspMap map;
    CHECK(map.load(path, {}));

    NavGraph graph;
    graph.build(map, {}, Vec3{-300, -300, -50}, Vec3{300, 300, 100});

    CHECK(!graph.empty()); // the grid should have found the floor
    for (const NavNode& n : graph.nodes()) {
        CHECK_NEAR(n.pos.z, 0.0, 10.0); // lands right at the floor plane, within the march step + standoff
    }
}

TEST(nav_graph_extracts_adjacent_floor_polygons_and_connects_them) {
    std::string path = buildTwoAdjacentFloorQuads();
    BspMap map;
    CHECK(map.load(path, {}));
    CHECK_EQ(map.faces().size(), (size_t)2);

    NavGraph graph;
    // Tiny bounds well away from the quads: no grid/seed noise, just the
    // two real floor polygons.
    graph.build(map, {}, Vec3{-1, -1, -1}, Vec3{1, 1, 1});

    CHECK_EQ(graph.nodes().size(), (size_t)2);
    for (const NavNode& n : graph.nodes()) CHECK(n.fromPolygon);

    // Centroids: quad A is (0,0)-(100,100) -> (50,50); quad B is
    // (100,0)-(200,100) -> (150,50).
    bool sawA = false, sawB = false;
    for (const NavNode& n : graph.nodes()) {
        if (std::abs(n.pos.x - 50.0f) < 1.0f) sawA = true;
        if (std::abs(n.pos.x - 150.0f) < 1.0f) sawB = true;
    }
    CHECK(sawA);
    CHECK(sawB);

    // They share an edge, so a path between their centroids should be a
    // direct one-hop route.
    std::vector<Vec3> route = graph.findPath(Vec3{50, 50, 0}, Vec3{150, 50, 0});
    CHECK_EQ(route.size(), (size_t)1);
}

TEST(nav_graph_filters_out_walls_and_tiny_floor_slivers) {
    std::string path = buildFloorWallAndTinyFaces();
    BspMap map;
    CHECK(map.load(path, {}));
    CHECK_EQ(map.faces().size(), (size_t)3);

    NavGraph graph;
    graph.build(map, {}, Vec3{-1, -1, -1}, Vec3{1, 1, 1});

    // Only face A (the real 100x100 floor) should survive: B is a wall
    // (not upward-facing), C is a floor but too small (10x10).
    CHECK_EQ(graph.nodes().size(), (size_t)1);
    if (!graph.nodes().empty()) {
        CHECK(graph.nodes()[0].fromPolygon);
        CHECK_NEAR(graph.nodes()[0].pos.x, 50.0, 1.0);
        CHECK_NEAR(graph.nodes()[0].pos.y, 50.0, 1.0);
    }
}

int main() { return RUN_ALL_TESTS(); }
