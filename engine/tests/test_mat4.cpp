#include "test_framework.h"

#include "mat4.h"

TEST(dot_and_cross_basic) {
    CHECK_NEAR(dot(Vec3f{1, 0, 0}, Vec3f{0, 1, 0}), 0.0, 1e-6);
    CHECK_NEAR(dot(Vec3f{2, 3, 4}, Vec3f{2, 3, 4}), 4.0 + 9.0 + 16.0, 1e-6);

    Vec3f c = cross(Vec3f{1, 0, 0}, Vec3f{0, 1, 0});
    CHECK_NEAR(c.x, 0.0, 1e-6);
    CHECK_NEAR(c.y, 0.0, 1e-6);
    CHECK_NEAR(c.z, 1.0, 1e-6);
}

TEST(normalize_zero_vector_is_zero) {
    Vec3f n = normalize(Vec3f{0, 0, 0});
    CHECK_NEAR(n.x, 0.0, 1e-6);
    CHECK_NEAR(n.y, 0.0, 1e-6);
    CHECK_NEAR(n.z, 0.0, 1e-6);
}

TEST(normalize_unit_length) {
    Vec3f n = normalize(Vec3f{3, 4, 0});
    CHECK_NEAR(n.x, 0.6, 1e-5);
    CHECK_NEAR(n.y, 0.8, 1e-5);
    CHECK_NEAR(dot(n, n), 1.0, 1e-5);
}

TEST(look_at_places_eye_at_origin_in_view_space) {
    // Looking from (0,0,5) at the origin, up = +Y: the eye should map to the
    // view-space origin, and "forward" (0,0,-5 in view space, i.e. -Z) should
    // land somewhere along -Z ahead of the camera.
    Mat4 view = lookAt(Vec3f{0, 0, 5}, Vec3f{0, 0, 0}, Vec3f{0, 1, 0});

    // Transform the eye point itself through the view matrix; it must map to (0,0,0).
    auto transform = [&](Vec3f p) {
        float x = view.m[0] * p.x + view.m[4] * p.y + view.m[8] * p.z + view.m[12];
        float y = view.m[1] * p.x + view.m[5] * p.y + view.m[9] * p.z + view.m[13];
        float z = view.m[2] * p.x + view.m[6] * p.y + view.m[10] * p.z + view.m[14];
        return Vec3f{x, y, z};
    };

    Vec3f eyeInView = transform(Vec3f{0, 0, 5});
    CHECK_NEAR(eyeInView.x, 0.0, 1e-4);
    CHECK_NEAR(eyeInView.y, 0.0, 1e-4);
    CHECK_NEAR(eyeInView.z, 0.0, 1e-4);

    Vec3f targetInView = transform(Vec3f{0, 0, 0});
    CHECK_NEAR(targetInView.x, 0.0, 1e-4);
    CHECK_NEAR(targetInView.y, 0.0, 1e-4);
    CHECK_NEAR(targetInView.z, -5.0, 1e-4); // target sits 5 units down -Z from the eye
}

TEST(perspective_projects_center_line_to_zero_xy) {
    Mat4 proj = perspective(90.0f, 1.0f, 1.0f, 100.0f);
    // A point straight down -Z should project to x=0,y=0 in clip space.
    float x = proj.m[0] * 0 + proj.m[4] * 0 + proj.m[8] * -10 + proj.m[12] * 1;
    float y = proj.m[1] * 0 + proj.m[5] * 0 + proj.m[9] * -10 + proj.m[13] * 1;
    CHECK_NEAR(x, 0.0, 1e-5);
    CHECK_NEAR(y, 0.0, 1e-5);
}

int main() { return RUN_ALL_TESTS(); }
