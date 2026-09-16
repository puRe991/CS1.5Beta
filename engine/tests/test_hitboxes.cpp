#include "test_framework.h"

#include "hitboxes.h"

TEST(damage_multiplier_matches_expected_balance) {
    CHECK_NEAR(hitboxDamageMultiplier(BodyPart::Head), 4.0f, 1e-6f);
    CHECK_NEAR(hitboxDamageMultiplier(BodyPart::Chest), 1.0f, 1e-6f);
    CHECK_NEAR(hitboxDamageMultiplier(BodyPart::Stomach), 1.25f, 1e-6f);
    CHECK_NEAR(hitboxDamageMultiplier(BodyPart::LeftArm), 1.0f, 1e-6f);
    CHECK_NEAR(hitboxDamageMultiplier(BodyPart::RightLeg), 0.75f, 1e-6f);
    CHECK_NEAR(hitboxDamageMultiplier(BodyPart::Generic), 1.0f, 1e-6f);
}

namespace {
WorldHitbox box(BodyPart part, float minx, float miny, float minz, float maxx, float maxy, float maxz) {
    WorldHitbox b;
    b.part = part;
    b.mins[0] = minx; b.mins[1] = miny; b.mins[2] = minz;
    b.maxs[0] = maxx; b.maxs[1] = maxy; b.maxs[2] = maxz;
    return b;
}
} // namespace

TEST(trace_hitboxes_hits_single_box_head_on) {
    std::vector<WorldHitbox> boxes = {box(BodyPart::Chest, 90, -10, -10, 110, 10, 10)};
    HitboxTraceResult r = traceHitboxes(boxes, Vec3{0, 0, 0}, Vec3{1, 0, 0}, 1000.0f);
    CHECK(r.hit);
    CHECK(r.part == BodyPart::Chest);
    CHECK_NEAR(r.distance, 90.0f, 1e-3f);
    CHECK_NEAR(r.damageMultiplier, 1.0f, 1e-6f);
}

TEST(trace_hitboxes_picks_the_nearest_of_several) {
    std::vector<WorldHitbox> boxes = {
        box(BodyPart::Chest, 200, -10, -10, 220, 10, 10), // farther
        box(BodyPart::Head, 90, -5, -5, 110, 5, 5),        // nearer
    };
    HitboxTraceResult r = traceHitboxes(boxes, Vec3{0, 0, 0}, Vec3{1, 0, 0}, 1000.0f);
    CHECK(r.hit);
    CHECK(r.part == BodyPart::Head);
    CHECK_NEAR(r.damageMultiplier, 4.0f, 1e-6f);
}

TEST(trace_hitboxes_misses_when_ray_passes_beside_box) {
    std::vector<WorldHitbox> boxes = {box(BodyPart::Chest, 90, 50, -10, 110, 70, 10)};
    HitboxTraceResult r = traceHitboxes(boxes, Vec3{0, 0, 0}, Vec3{1, 0, 0}, 1000.0f);
    CHECK(!r.hit);
}

TEST(trace_hitboxes_respects_max_distance) {
    std::vector<WorldHitbox> boxes = {box(BodyPart::Chest, 500, -10, -10, 520, 10, 10)};
    HitboxTraceResult r = traceHitboxes(boxes, Vec3{0, 0, 0}, Vec3{1, 0, 0}, 100.0f);
    CHECK(!r.hit);
}

TEST(trace_hitboxes_handles_axis_aligned_ray) {
    // Ray travels purely along Y; must not divide by zero on the X/Z axes.
    std::vector<WorldHitbox> boxes = {box(BodyPart::Head, -5, 40, -5, 5, 60, 5)};
    HitboxTraceResult r = traceHitboxes(boxes, Vec3{0, 0, 0}, Vec3{0, 1, 0}, 1000.0f);
    CHECK(r.hit);
    CHECK_NEAR(r.distance, 40.0f, 1e-3f);
}

int main() { return RUN_ALL_TESTS(); }
