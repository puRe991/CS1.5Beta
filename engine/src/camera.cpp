#include "camera.h"
#include <cmath>

static constexpr float kMoveSpeed = 200.0f; // units/sec, GoldSrc maps use ~inch-scale units
static constexpr float kLookSpeed = 0.15f;  // degrees/pixel

void Camera::move(float forward, float strafe, float up, float dt) {
    float yawRad = yaw * 3.14159265f / 180.0f;
    float fx = std::cos(yawRad), fy = std::sin(yawRad);
    float rx = -fy, ry = fx;

    x += (fx * forward + rx * strafe) * kMoveSpeed * dt;
    y += (fy * forward + ry * strafe) * kMoveSpeed * dt;
    z += up * kMoveSpeed * dt;
}

void Camera::look(float dxPixels, float dyPixels) {
    yaw += dxPixels * kLookSpeed;
    pitch -= dyPixels * kLookSpeed;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
