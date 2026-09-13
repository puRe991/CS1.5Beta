#pragma once

#include <vector>

#include "assets/bsp.h"

// Simulates the map's moving/destructible brush entities — func_door,
// func_plat (moving platforms), and func_breakable — on top of the static
// BspMap collision/rendering that entities.h/EntitySystem doesn't touch
// (see its header comment: this is the "func_door/func_button logic,
// breakables" follow-up it names).
//
// Each entity keeps its own current offset from the compiled/rest position
// baked into the BSP; collision against it is done by testing (point -
// offset) against that submodel's own clip tree (BspMap::pointInSolidModel),
// so a moved door/platform collides where it actually is, not where it was
// compiled.

enum class BrushKind { Door, Platform, Breakable };
enum class DoorState { Closed, Opening, Open, Closing };
enum class PlatformState { Top, MovingDown, Bottom, MovingUp };

struct BrushEntity {
    BrushKind kind = BrushKind::Door;
    int modelIndex = -1;
    Vec3 mins{}, maxs{}; // rest-position (offset == 0) bounds, from BspModelBounds

    // Door / platform motion.
    Vec3 moveDir{0, 0, 0};      // normalized direction of travel
    float travelDistance = 0.0f; // how far along moveDir it travels
    float speed = 100.0f;        // units/sec
    float wait = 3.0f;            // seconds parked open/down before returning
    Vec3 offset{0, 0, 0};         // current offset from rest position
    Vec3 prevOffset{0, 0, 0};     // last frame's offset, for platform-carry delta
    float waitTimer = 0.0f;
    DoorState doorState = DoorState::Closed;
    PlatformState platState = PlatformState::Top;

    // Breakable.
    int health = 15;
    bool destroyed = false;

    // Faces (indices into BspMap::faces()) belonging to this entity's model,
    // used to draw it at its current offset instead of statically.
    std::vector<int> faceIndices;

    Vec3 worldMins() const { return {mins.x + offset.x, mins.y + offset.y, mins.z + offset.z}; }
    Vec3 worldMaxs() const { return {maxs.x + offset.x, maxs.y + offset.y, maxs.z + offset.z}; }
};

struct BrushEntitySystem {
    std::vector<BrushEntity> entities;

    void build(const BspMap& map);

    // Advances every entity's state machine by dt. playerMins/playerMaxs is
    // the player's current world-space AABB, used to trigger doors (touch)
    // and platforms (stepping onto the top). outCarryDelta is set to the
    // movement delta (this frame) of whichever platform the player is
    // currently standing on, so the caller can carry the player along
    // instead of leaving them floating as the platform moves out from
    // under them.
    void update(float dt, const BspMap& map, Vec3 playerMins, Vec3 playerMaxs, Vec3& outCarryDelta);

    // Solid collision test against every non-destroyed entity, matching
    // BspMap::pointInSolidHull's hull numbering — call this alongside it.
    bool pointInSolid(const BspMap& map, Vec3 point, int hull, Vec3* outNormal = nullptr) const;

    // Applies `damage` to any breakable whose current world AABB contains
    // `point` (a bullet hit); destroys it (stops colliding/rendering) once
    // health drops to 0 or below. Returns true if something was hit.
    bool damageAt(Vec3 point, int damage);
};
