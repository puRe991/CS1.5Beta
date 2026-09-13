#include "camera.h"
#include "settings.h"
#include <cmath>

// Base degrees/pixel at sensitivity 1.0, matching the old hardcoded 0.15
// look speed. "sensitivity" is the player-facing knob (config.cfg, GoldSrc
// naming); "m_pitch" flips vertical look for players who want inverted mouse.
static constexpr float kBaseLookSpeed = 0.05f;

void Camera::look(float dxPixels, float dyPixels, float sensitivityScale) {
    float lookSpeed = kBaseLookSpeed * sensitivity.AsFloat() * sensitivityScale;

    // Optional acceleration curve: faster physical mouse movement gets an
    // extra boost on top of the flat multiplier, up to +100% at high speed.
    if (m_customaccel.AsBool()) {
        float accelX = std::min(1.0f, std::fabs(dxPixels) / 40.0f);
        float accelY = std::min(1.0f, std::fabs(dyPixels) / 40.0f);
        yaw += dxPixels * lookSpeed * (1.0f + accelX);
        pitch -= dyPixels * lookSpeed * m_pitch.AsFloat() * (1.0f + accelY);
    } else {
        yaw += dxPixels * lookSpeed;
        pitch -= dyPixels * lookSpeed * m_pitch.AsFloat();
    }

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
