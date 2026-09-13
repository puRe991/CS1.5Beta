#include "camera.h"

static constexpr float kLookSpeed = 0.15f; // degrees/pixel

void Camera::look(float dxPixels, float dyPixels) {
    yaw += dxPixels * kLookSpeed;
    pitch -= dyPixels * kLookSpeed;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
