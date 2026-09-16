#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "assets/bsp.h"
#include "assets/mdl.h"

// Per-body-part damage multipliers, applied on top of a weapon's base
// `damage` (weapons.h) once a hitscan trace resolves to a specific
// MdlHitbox/WorldHitbox instead of just "hit something". Functional game
// balance numbers CS is generally known for (head shots ~4x, torso 1x,
// limbs weaker), not copied text — same convention weapons.h already uses.
inline float hitboxDamageMultiplier(BodyPart part) {
    switch (part) {
        case BodyPart::Head:     return 4.0f;
        case BodyPart::Chest:    return 1.0f;
        case BodyPart::Stomach:  return 1.25f;
        case BodyPart::LeftArm:
        case BodyPart::RightArm: return 1.0f;
        case BodyPart::LeftLeg:
        case BodyPart::RightLeg: return 0.75f;
        default:                 return 1.0f; // Generic
    }
}

struct HitboxTraceResult {
    bool hit = false;
    int boxIndex = -1;        // index into the boxes[] passed to traceHitboxes()
    BodyPart part = BodyPart::Generic;
    float distance = 0.0f;    // along the ray, from start
    float damageMultiplier = 1.0f;
};

// Finds the nearest WorldHitbox a ray hits, using the standard slab method
// against each box's axis-aligned world bounds. `dir` must be normalized;
// only hits within [0, maxDistance] count. This is the actual per-body-part
// hit detection the README TODO calls for — resolving *which* hitbox a
// bullet crossed, not just whether the ray hit *something* (that part is
// BspMap::traceLine, already used for world geometry).
inline HitboxTraceResult traceHitboxes(const std::vector<WorldHitbox>& boxes, Vec3 start, Vec3 dir, float maxDistance) {
    HitboxTraceResult best;
    float bestT = maxDistance;

    for (size_t i = 0; i < boxes.size(); ++i) {
        const WorldHitbox& box = boxes[i];
        float tMin = 0.0f, tMax = maxDistance;
        bool boxHit = true;

        const float origin[3] = {start.x, start.y, start.z};
        const float direction[3] = {dir.x, dir.y, dir.z};
        for (int axis = 0; axis < 3 && boxHit; ++axis) {
            if (std::fabs(direction[axis]) < 1e-8f) {
                // Ray parallel to this axis: must already be within the slab.
                if (origin[axis] < box.mins[axis] || origin[axis] > box.maxs[axis]) boxHit = false;
                continue;
            }
            float invD = 1.0f / direction[axis];
            float t0 = (box.mins[axis] - origin[axis]) * invD;
            float t1 = (box.maxs[axis] - origin[axis]) * invD;
            if (t0 > t1) std::swap(t0, t1);
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMin > tMax) boxHit = false;
        }

        if (boxHit && tMin <= bestT) {
            bestT = tMin;
            best.hit = true;
            best.boxIndex = (int)i;
            best.part = box.part;
            best.distance = tMin;
            best.damageMultiplier = hitboxDamageMultiplier(box.part);
        }
    }

    return best;
}
