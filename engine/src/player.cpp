#include "player.h"

namespace {

constexpr float kGravity = 800.0f;   // units/sec^2, Source-ish
constexpr float kJumpSpeed = 300.0f; // units/sec, initial upward velocity
constexpr float kGroundProbe = 2.0f; // how far below the feet to test for floor

} // namespace

void playerStep(Camera& camera, PlayerState& state, const BspMap& map,
                const PlayerInput& input, float dt) {
    float dx, dy, dzUnused;
    camera.wishDelta(input.forward, input.strafe, 0.0f, dt, dx, dy, dzUnused);

    // Resolve X and Y independently so movement slides along a wall instead of
    // stopping dead the moment either axis is blocked.
    Vec3 candidate{camera.x, camera.y, camera.z};
    candidate.x += dx;
    if (map.pointInSolid(candidate)) candidate.x = camera.x;
    candidate.y += dy;
    if (map.pointInSolid(candidate)) candidate.y = camera.y;

    Vec3 groundProbe = candidate;
    groundProbe.z -= kGroundProbe;
    state.grounded = map.pointInSolid(groundProbe);

    if (input.jumpPressed && state.grounded) {
        state.velocityZ = kJumpSpeed;
        state.grounded = false;
    } else if (state.grounded && state.velocityZ <= 0.0f) {
        state.velocityZ = 0.0f;
    } else {
        state.velocityZ -= kGravity * dt;
    }

    candidate.z += state.velocityZ * dt;
    if (map.pointInSolid(candidate)) {
        // Hit floor or ceiling: cancel this step's vertical move entirely
        // rather than resolving the exact contact point.
        if (state.velocityZ < 0.0f) state.grounded = true;
        state.velocityZ = 0.0f;
        candidate.z = camera.z;
    }

    camera.x = candidate.x;
    camera.y = candidate.y;
    camera.z = candidate.z;
}
