#pragma once

// Z-up convention to match GoldSrc/BSP map coordinates directly (no axis conversion needed).
struct Camera {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float yaw = 0.0f;   // rotation in the XY plane, degrees
    float pitch = 0.0f; // up/down, degrees

    void move(float forward, float strafe, float up, float dt);
    void look(float dxPixels, float dyPixels);
};
