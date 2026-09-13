#pragma once

// Z-up convention to match GoldSrc/BSP map coordinates directly (no axis conversion needed).
struct Camera {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float yaw = 0.0f;   // rotation in the XY plane, degrees
    float pitch = 0.0f; // up/down, degrees

    // sensitivityScale multiplies the base "sensitivity" cvar on top (e.g.
    // for aim-down-sights, which applies m_ads_sensitivity here).
    void look(float dxPixels, float dyPixels, float sensitivityScale = 1.0f);
};
