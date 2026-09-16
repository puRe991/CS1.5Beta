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

int main() { return RUN_ALL_TESTS(); }
