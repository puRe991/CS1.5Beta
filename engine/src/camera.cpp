#include "camera.h"
#include <cmath>

static constexpr float kMoveSpeed = 3.0f;   // units/sec
static constexpr float kLookSpeed = 0.15f;  // degrees/pixel

void Camera::move(float forward, float strafe, float dt) {
    float yawRad = yaw * 3.14159265f / 180.0f;
    float fx = std::cos(yawRad), fz = std::sin(yawRad);
    float rx = -fz, rz = fx;

    x += (fx * forward + rx * strafe) * kMoveSpeed * dt;
    z += (fz * forward + rz * strafe) * kMoveSpeed * dt;
}

void Camera::look(float dxPixels, float dyPixels) {
    yaw += dxPixels * kLookSpeed;
    pitch -= dyPixels * kLookSpeed;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
