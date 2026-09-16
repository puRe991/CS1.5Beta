#include "test_framework.h"

#include "camera.h"

#include <cmath>

TEST(wish_delta_forward_at_zero_yaw_moves_along_x) {
    Camera cam;
    cam.yaw = 0.0f;
    float dx, dy, dz;
    cam.wishDelta(/*forward=*/1.0f, /*strafe=*/0.0f, /*up=*/0.0f, /*dt=*/1.0f, dx, dy, dz);
    CHECK(dx > 0.0f);
    CHECK_NEAR(dy, 0.0, 1e-3);
    CHECK_NEAR(dz, 0.0, 1e-6);
}

TEST(wish_delta_up_only_moves_along_z_regardless_of_yaw) {
    Camera cam;
    cam.yaw = 37.0f;
    float dx, dy, dz;
    cam.wishDelta(0.0f, 0.0f, 1.0f, 2.0f, dx, dy, dz);
    CHECK_NEAR(dx, 0.0, 1e-6);
    CHECK_NEAR(dy, 0.0, 1e-6);
    CHECK(dz > 0.0f);
}

TEST(wish_delta_scales_with_dt) {
    Camera cam;
    cam.yaw = 0.0f;
    float dx1, dy1, dz1, dx2, dy2, dz2;
    cam.wishDelta(1.0f, 0.0f, 0.0f, 1.0f, dx1, dy1, dz1);
    cam.wishDelta(1.0f, 0.0f, 0.0f, 2.0f, dx2, dy2, dz2);
    CHECK_NEAR(dx2, dx1 * 2.0, 1e-3);
}

// Regression: holding forward and strafe together used to produce a vector of
// length sqrt(2), making diagonal movement ~41% faster than straight ahead.
TEST(wish_delta_diagonal_is_not_faster_than_straight) {
    Camera cam;
    cam.yaw = 0.0f;

    float fx, fy, fz;
    cam.wishDelta(1.0f, 0.0f, 0.0f, 1.0f, fx, fy, fz);
    float straightSpeed = std::sqrt(fx * fx + fy * fy);

    float dx, dy, dz;
    cam.wishDelta(1.0f, 1.0f, 0.0f, 1.0f, dx, dy, dz);
    float diagonalSpeed = std::sqrt(dx * dx + dy * dy);

    CHECK_NEAR(diagonalSpeed, straightSpeed, 1e-3);
}

// Partial (sub-unit) input should still mean partial speed — the clamp must
// only shorten vectors longer than 1, never stretch shorter ones.
TEST(wish_delta_preserves_partial_input) {
    Camera cam;
    cam.yaw = 0.0f;

    float fx, fy, fz;
    cam.wishDelta(1.0f, 0.0f, 0.0f, 1.0f, fx, fy, fz);
    float fullSpeed = std::sqrt(fx * fx + fy * fy);

    float hx, hy, hz;
    cam.wishDelta(0.5f, 0.0f, 0.0f, 1.0f, hx, hy, hz);
    float halfSpeed = std::sqrt(hx * hx + hy * hy);

    CHECK_NEAR(halfSpeed, fullSpeed * 0.5, 1e-3);
}

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
    CHECK_NEAR(cam.yaw, 3.0, 1.0); // 0.15 deg/pixel * 20 px = 3 degrees
}

int main() { return RUN_ALL_TESTS(); }
