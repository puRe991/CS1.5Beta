#pragma once

// Z-up convention to match GoldSrc/BSP map coordinates directly (no axis conversion needed).
struct Camera {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float yaw = 0.0f;   // rotation in the XY plane, degrees
    float pitch = 0.0f; // up/down, degrees

    // Returns the movement delta for this frame without applying it, so the
    // caller can collision-test the candidate position first.
    void wishDelta(float forward, float strafe, float up, float dt, float& dx, float& dy, float& dz) const;
    void look(float dxPixels, float dyPixels);
};
