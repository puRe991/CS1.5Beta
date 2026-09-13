#include "brush_entities.h"

#include <cmath>
#include <cstdlib>

namespace {

constexpr float kDeg2Rad = 3.14159265358979323846f / 180.0f;

float keyFloat(const BspEntity& ent, const char* key, float fallback) {
    const std::string* v = ent.get(key);
    return v ? (float)std::atof(v->c_str()) : fallback;
}

// Classic Quake/GoldSrc SetMovedir: "angle" -1 = straight up, -2 = straight
// down, anything else = a yaw angle in the XY plane.
Vec3 moveDirFromAngle(float angle) {
    if (angle == -1.0f) return Vec3{0, 0, 1};
    if (angle == -2.0f) return Vec3{0, 0, -1};
    return Vec3{std::cos(angle * kDeg2Rad), std::sin(angle * kDeg2Rad), 0.0f};
}

bool aabbOverlap(Vec3 aMin, Vec3 aMax, Vec3 bMin, Vec3 bMax) {
    return aMin.x <= bMax.x && aMax.x >= bMin.x &&
           aMin.y <= bMax.y && aMax.y >= bMin.y &&
           aMin.z <= bMax.z && aMax.z >= bMin.z;
}

Vec3 addScaled(Vec3 a, Vec3 dir, float s) {
    return Vec3{a.x + dir.x * s, a.y + dir.y * s, a.z + dir.z * s};
}

float vecLen(Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

} // namespace

void BrushEntitySystem::build(const BspMap& map) {
    entities.clear();

    for (const auto& ent : map.entities()) {
        const std::string* classname = ent.get("classname");
        if (!classname) continue;
        bool isDoor = *classname == "func_door";
        bool isPlat = *classname == "func_plat";
        bool isBreak = *classname == "func_breakable";
        if (!isDoor && !isPlat && !isBreak) continue;

        int modelIndex = BspMap::modelIndexFor(ent);
        if (modelIndex < 0 || (size_t)modelIndex >= map.models().size()) continue;

        BrushEntity e;
        e.modelIndex = modelIndex;
        e.mins = map.models()[modelIndex].mins;
        e.maxs = map.models()[modelIndex].maxs;
        e.speed = keyFloat(ent, "speed", isPlat ? 150.0f : 100.0f);
        e.wait = keyFloat(ent, "wait", 3.0f);
        if (e.wait < 0.0f) e.wait = 1e9f; // "-1" (no auto-close) — no button/trigger yet to reopen manually

        Vec3 size{e.maxs.x - e.mins.x, e.maxs.y - e.mins.y, e.maxs.z - e.mins.z};

        if (isDoor) {
            e.kind = BrushKind::Door;
            float angle = keyFloat(ent, "angle", 0.0f);
            e.moveDir = moveDirFromAngle(angle);
            float lip = keyFloat(ent, "lip", 8.0f);
            float extent = std::fabs(e.moveDir.x) * size.x + std::fabs(e.moveDir.y) * size.y +
                           std::fabs(e.moveDir.z) * size.z;
            e.travelDistance = std::max(extent - lip, 0.0f);
        } else if (isPlat) {
            e.kind = BrushKind::Platform;
            e.moveDir = Vec3{0, 0, -1};
            float height = keyFloat(ent, "height", 0.0f);
            e.travelDistance = height > 0.0f ? height : std::max(size.z - 8.0f, 0.0f);
        } else {
            e.kind = BrushKind::Breakable;
            e.health = (int)keyFloat(ent, "health", 15.0f);
        }

        for (int32_t fi = 0; fi < (int32_t)map.faceModelIndices().size(); ++fi) {
            if (map.faceModelIndices()[fi] == modelIndex) e.faceIndices.push_back(fi);
        }

        entities.push_back(std::move(e));
    }
}

void BrushEntitySystem::update(float dt, const BspMap& map, Vec3 playerMins, Vec3 playerMaxs,
                                Vec3& outCarryDelta) {
    outCarryDelta = Vec3{0, 0, 0};

    for (auto& e : entities) {
        e.prevOffset = e.offset;
        if (e.kind == BrushKind::Breakable) continue; // static until destroyed by damage

        bool touching = aabbOverlap(playerMins, playerMaxs, e.worldMins(), e.worldMaxs());

        if (e.kind == BrushKind::Door) {
            switch (e.doorState) {
                case DoorState::Closed:
                    if (touching) e.doorState = DoorState::Opening;
                    break;
                case DoorState::Opening: {
                    Vec3 target = addScaled(Vec3{0, 0, 0}, e.moveDir, e.travelDistance);
                    Vec3 toTarget{target.x - e.offset.x, target.y - e.offset.y, target.z - e.offset.z};
                    float remaining = vecLen(toTarget);
                    float step = e.speed * dt;
                    if (step >= remaining) {
                        e.offset = target;
                        e.doorState = DoorState::Open;
                        e.waitTimer = 0.0f;
                    } else {
                        e.offset = addScaled(e.offset, e.moveDir, step);
                    }
                    break;
                }
                case DoorState::Open:
                    e.waitTimer += dt;
                    if (e.waitTimer >= e.wait) e.doorState = DoorState::Closing;
                    break;
                case DoorState::Closing: {
                    if (touching) { e.doorState = DoorState::Opening; break; }
                    float remaining = vecLen(e.offset);
                    float step = e.speed * dt;
                    if (step >= remaining) {
                        e.offset = Vec3{0, 0, 0};
                        e.doorState = DoorState::Closed;
                    } else {
                        e.offset = addScaled(e.offset, e.moveDir, -step);
                    }
                    break;
                }
            }
        } else { // Platform
            // "Standing on top" — player's feet at/just above the platform's
            // current top surface, with horizontal overlap.
            Vec3 wmin = e.worldMins(), wmax = e.worldMaxs();
            bool onTop = playerMins.x <= wmax.x && playerMaxs.x >= wmin.x &&
                         playerMins.y <= wmax.y && playerMaxs.y >= wmin.y &&
                         playerMins.z >= wmax.z - 4.0f && playerMins.z <= wmax.z + 4.0f;

            switch (e.platState) {
                case PlatformState::Top:
                    if (onTop) e.platState = PlatformState::MovingDown;
                    break;
                case PlatformState::MovingDown: {
                    float remaining = e.travelDistance - (-e.offset.z);
                    float step = e.speed * dt;
                    if (step >= remaining) {
                        e.offset.z = -e.travelDistance;
                        e.platState = PlatformState::Bottom;
                        e.waitTimer = 0.0f;
                    } else {
                        e.offset.z -= step;
                    }
                    break;
                }
                case PlatformState::Bottom:
                    e.waitTimer += dt;
                    if (e.waitTimer >= e.wait) e.platState = PlatformState::MovingUp;
                    break;
                case PlatformState::MovingUp: {
                    float remaining = -e.offset.z;
                    float step = e.speed * dt;
                    if (step >= remaining) {
                        e.offset.z = 0.0f;
                        e.platState = PlatformState::Top;
                    } else {
                        e.offset.z += step;
                    }
                    break;
                }
            }

            if (onTop && (e.platState == PlatformState::MovingDown || e.platState == PlatformState::MovingUp)) {
                outCarryDelta.z += e.offset.z - e.prevOffset.z;
            }
        }
    }
    (void)map;
}

bool BrushEntitySystem::pointInSolid(const BspMap& map, Vec3 point, int hull, Vec3* outNormal) const {
    for (const auto& e : entities) {
        if (e.kind == BrushKind::Breakable && e.destroyed) continue;
        Vec3 local{point.x - e.offset.x, point.y - e.offset.y, point.z - e.offset.z};
        if (map.pointInSolidModel(local, e.modelIndex, hull, outNormal)) return true;
    }
    return false;
}

bool BrushEntitySystem::damageAt(Vec3 point, int damage) {
    bool hit = false;
    constexpr float kPad = 4.0f;
    for (auto& e : entities) {
        if (e.kind != BrushKind::Breakable || e.destroyed) continue;
        Vec3 wmin = e.worldMins(), wmax = e.worldMaxs();
        if (point.x >= wmin.x - kPad && point.x <= wmax.x + kPad &&
            point.y >= wmin.y - kPad && point.y <= wmax.y + kPad &&
            point.z >= wmin.z - kPad && point.z <= wmax.z + kPad) {
            e.health -= damage;
            hit = true;
            if (e.health <= 0) e.destroyed = true;
        }
    }
    return hit;
}
