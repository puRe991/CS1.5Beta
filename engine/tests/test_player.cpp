#include "test_framework.h"
#include "fixtures.h"

#include "player.h"

namespace {

// A map whose hull 1 is solid everywhere below z=0 and empty above it: one
// plane with normal +Z at dist 0, empty on the +Z side, solid on the -Z side.
std::string buildFloorMap() {
    fixtures::BspBuilder builder;
    fixtures::BspDPlane plane{};
    plane.normal[2] = 1;
    plane.dist = 0;
    builder.planes.push_back(plane);

    fixtures::BspDClipNode node{};
    node.planeNum = 0;
    node.children[0] = -1; // CONTENTS_EMPTY above the floor
    node.children[1] = -2; // CONTENTS_SOLID below it
    builder.clipNodes.push_back(node);

    fixtures::BspDModel model{};
    model.headNode[1] = 0;
    builder.models.push_back(model);
    return builder.build();
}

// A map with no collision at all, so the player falls freely forever.
std::string buildEmptyMap() {
    fixtures::BspBuilder builder;
    return builder.build();
}

} // namespace

TEST(player_falls_under_gravity_in_open_space) {
    std::string path = buildEmptyMap();
    BspMap map;
    CHECK(map.load(path, {}));

    Camera cam;
    cam.z = 100.0f;
    PlayerState state;

    playerStep(cam, state, map, PlayerInput{}, 0.1f);

    CHECK(state.velocityZ < 0.0f); // gravity pulled downward
    CHECK(cam.z < 100.0f);         // and actually moved the player down
    CHECK(!state.grounded);
}

TEST(player_stands_on_floor_without_sinking) {
    std::string path = buildFloorMap();
    BspMap map;
    CHECK(map.load(path, {}));

    Camera cam;
    cam.z = 1.0f; // just above the floor, within the 2-unit ground probe
    PlayerState state;

    for (int i = 0; i < 20; ++i) {
        playerStep(cam, state, map, PlayerInput{}, 1.0f / 60.0f);
    }

    CHECK(state.grounded);
    CHECK_NEAR(state.velocityZ, 0.0, 1e-3);
    CHECK_NEAR(cam.z, 1.0, 1e-3); // held in place, no slow sink
}

TEST(player_jump_leaves_the_ground_then_comes_back_down) {
    std::string path = buildFloorMap();
    BspMap map;
    CHECK(map.load(path, {}));

    Camera cam;
    cam.z = 1.0f;
    PlayerState state;

    // Settle on the floor first.
    playerStep(cam, state, map, PlayerInput{}, 1.0f / 60.0f);
    CHECK(state.grounded);

    PlayerInput jump;
    jump.jumpPressed = true;
    playerStep(cam, state, map, jump, 1.0f / 60.0f);

    CHECK(state.velocityZ > 0.0f); // launched upward
    float peakZ = cam.z;
    CHECK(peakZ > 1.0f);

    // Without holding jump, gravity should bring the player back to the floor.
    for (int i = 0; i < 300; ++i) {
        playerStep(cam, state, map, PlayerInput{}, 1.0f / 60.0f);
    }
    CHECK(state.grounded);
    CHECK(cam.z < peakZ);
}

TEST(player_cannot_jump_while_airborne) {
    std::string path = buildEmptyMap();
    BspMap map;
    CHECK(map.load(path, {}));

    Camera cam;
    cam.z = 500.0f;
    PlayerState state;

    PlayerInput jump;
    jump.jumpPressed = true;
    playerStep(cam, state, map, jump, 1.0f / 60.0f);

    // No ground under the player, so the jump must not apply any upward impulse.
    CHECK(state.velocityZ < 0.0f);
    CHECK(!state.grounded);
}

TEST(player_horizontal_move_is_blocked_by_solid_geometry) {
    // Solid on the -X side of the x=0 plane, empty on the +X side.
    fixtures::BspBuilder builder;
    fixtures::BspDPlane plane{};
    plane.normal[0] = 1;
    plane.dist = 0;
    builder.planes.push_back(plane);

    fixtures::BspDClipNode node{};
    node.planeNum = 0;
    node.children[0] = -1; // empty, +X
    node.children[1] = -2; // solid, -X
    builder.clipNodes.push_back(node);

    fixtures::BspDModel model{};
    model.headNode[1] = 0;
    builder.models.push_back(model);

    std::string path = builder.build();
    BspMap map;
    CHECK(map.load(path, {}));

    Camera cam;
    cam.x = 10.0f;
    cam.z = 100.0f;
    cam.yaw = 180.0f; // face -X, straight at the solid half

    PlayerState state;
    PlayerInput walk;
    walk.forward = 1.0f;

    for (int i = 0; i < 120; ++i) {
        playerStep(cam, state, map, walk, 1.0f / 60.0f);
    }

    // Movement into the solid side is rejected, so the player never crosses x=0.
    CHECK(cam.x >= 0.0f);
}

int main() { return RUN_ALL_TESTS(); }
