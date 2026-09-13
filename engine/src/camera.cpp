#include "camera.h"
#include "cvar.h"

// Base degrees/pixel at sensitivity 1.0, matching the old hardcoded 0.15
// look speed. "sensitivity" is the player-facing knob (config.cfg, GoldSrc
// naming); "m_pitch" flips vertical look for players who want inverted mouse.
static constexpr float kBaseLookSpeed = 0.05f;
static Cvar sensitivity("sensitivity", "3.0", CVAR_ARCHIVE, "mouse look speed multiplier");
static Cvar m_pitch("m_pitch", "1.0", CVAR_ARCHIVE, "vertical mouse look scale; negative inverts");

void Camera::look(float dxPixels, float dyPixels) {
    float lookSpeed = kBaseLookSpeed * sensitivity.AsFloat();
    yaw += dxPixels * lookSpeed;
    pitch -= dyPixels * lookSpeed * m_pitch.AsFloat();

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
