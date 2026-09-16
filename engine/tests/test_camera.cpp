#include "test_framework.h"

#include "camera.h"
#include "cvar.h"

#include <cmath>

// Camera is now purely orientation: movement moved into the player physics in
// main.cpp (friction, ground/air acceleration, ducking), which normalizes the
// wish direction there. What's left to test here is mouse look.

TEST(look_clamps_pitch_to_89_degrees) {
    Camera cam;
    cam.look(0.0f, -100000.0f); // huge upward look
    CHECK(cam.pitch <= 89.0f);
    CHECK(cam.pitch >= 88.9f);

    cam.look(0.0f, 200000.0f); // huge downward look
    CHECK(cam.pitch >= -89.0f);
    CHECK(cam.pitch <= -88.9f);
}

TEST(look_accumulates_yaw) {
    Camera cam;
    cam.look(10.0f, 0.0f);
    cam.look(10.0f, 0.0f);
    // Default sensitivity 3.0 * 0.05 base = 0.15 deg/pixel, so 20 px = 3 degrees.
    CHECK_NEAR(cam.yaw, 3.0, 1e-3);
}

TEST(look_scales_with_sensitivity_cvar) {
    Cvar* sensitivity = CvarSystem::Get().Find("sensitivity");
    CHECK(sensitivity != nullptr);
    if (!sensitivity) return;

    Camera slow;
    sensitivity->SetString("1.0");
    slow.look(100.0f, 0.0f);

    Camera fast;
    sensitivity->SetString("2.0");
    fast.look(100.0f, 0.0f);

    CHECK_NEAR(fast.yaw, slow.yaw * 2.0, 1e-3);

    sensitivity->SetString("3.0"); // restore the default for other tests
}

// m_pitch inverts vertical look; a negative value must flip the sign.
TEST(look_respects_inverted_m_pitch) {
    Cvar* mPitch = CvarSystem::Get().Find("m_pitch");
    CHECK(mPitch != nullptr);
    if (!mPitch) return;

    Camera normal;
    normal.look(0.0f, 50.0f);

    mPitch->SetString("-1.0");
    Camera inverted;
    inverted.look(0.0f, 50.0f);

    CHECK_NEAR(inverted.pitch, -normal.pitch, 1e-3);

    mPitch->SetString("1.0");
}

int main() { return RUN_ALL_TESTS(); }
