#pragma once

struct Camera {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float yaw = 0.0f;   // left/right, degrees
    float pitch = 0.0f; // up/down, degrees

    void move(float forward, float strafe, float dt);
    void look(float dxPixels, float dyPixels);
};
