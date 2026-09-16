#pragma once

#include "assets/bsp.h"
#include "camera.h"

// Player movement resolved against the map's collision hull: horizontal
// movement with wall sliding, ground detection, jumping and gravity.
// Kept separate from the render loop so it can be exercised without a window.

struct PlayerState {
    float velocityZ = 0.0f;
    bool grounded = false;
};

struct PlayerInput {
    float forward = 0.0f; // +1 forward, -1 back
    float strafe = 0.0f;  // +1 right, -1 left
    bool jumpPressed = false; // true only on the frame jump was pressed
};

// Advances the player by one frame, updating `camera`'s position and `state`
// in place. `camera.z` is the collision origin (feet), matching the hull the
// BSP's clipnodes are built for.
void playerStep(Camera& camera, PlayerState& state, const BspMap& map,
                const PlayerInput& input, float dt);
