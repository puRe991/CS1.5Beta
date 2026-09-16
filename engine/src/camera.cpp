#include "camera.h"
#include <cmath>

static constexpr float kMoveSpeed = 200.0f; // units/sec, GoldSrc maps use ~inch-scale units
static constexpr float kLookSpeed = 0.15f;  // degrees/pixel

void Camera::wishDelta(float forward, float strafe, float up, float dt, float& dx, float& dy, float& dz) const {
    // Clamp the input vector to unit length so holding two directions at once
    // doesn't move sqrt(2)x faster than a single one. Shorter (analog) inputs
    // are left alone, so partial input still means partial speed.
    float inputLen = std::sqrt(forward * forward + strafe * strafe);
    if (inputLen > 1.0f) {
        forward /= inputLen;
        strafe /= inputLen;
    }

    float yawRad = yaw * 3.14159265f / 180.0f;
    float fx = std::cos(yawRad), fy = std::sin(yawRad);
    float rx = -fy, ry = fx;

    dx = (fx * forward + rx * strafe) * kMoveSpeed * dt;
    dy = (fy * forward + ry * strafe) * kMoveSpeed * dt;
    dz = up * kMoveSpeed * dt;
}

void Camera::look(float dxPixels, float dyPixels) {
    yaw += dxPixels * kLookSpeed;
    pitch -= dyPixels * kLookSpeed;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
