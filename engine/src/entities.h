#pragma once

#include <vector>

#include "assets/bsp.h"

// Classifies the BSP entity lump's raw key/value soup into typed gameplay
// data. Deliberately scoped to what's needed for a first playable test —
// spawn points and the two zone types (bomb target, buy zone) — not a
// general-purpose entity/scripting system (see README TODO for the rest:
// func_door/func_button logic, breakables, triggers, etc).
enum class Team { Unknown, T, CT };

struct SpawnPoint {
    Vec3 origin;
    float yaw = 0.0f;
    Team team = Team::Unknown;
};

// Axis-aligned bounding box of a brush entity (bomb target, buy zone, ...),
// taken directly from the BSP model bounds — good enough for "is the player
// standing in this zone", not for rendering the brush's actual shape.
struct ZoneRegion {
    Vec3 mins, maxs;
    Team team = Team::Unknown; // Unknown = applies to both teams (e.g. bomb targets)
};

bool pointInZone(const ZoneRegion& zone, Vec3 p);

struct EntitySystem {
    std::vector<SpawnPoint> spawns;
    std::vector<ZoneRegion> bombTargets;
    std::vector<ZoneRegion> buyZones;

    void build(const BspMap& map);
};
