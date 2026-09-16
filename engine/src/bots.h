#pragma once

#include <vector>

#include "assets/bsp.h"
#include "assets/mdl.h"
#include "brush_entities.h"
#include "entities.h"
#include "game_state.h"

// A first, real AI opponent — not a stub. Explicitly *not* covering the
// rest of the Bots & AI README TODO: one shared difficulty (no tiers),
// perception is a single line-of-sight raycast + max view distance (no
// simulated hearing, no reaction-time delay), and movement is straight-line
// steering with the same per-axis wall-slide collision the player uses —
// no navmesh/waypoint graph, so a bot can get stuck on complex geometry a
// real path wouldn't. No buy decisions, site attack/defense, or
// coordination between bots either. What *is* real: perception, movement,
// firing (with actual damage to the player), and being shot back (via the
// hitbox system in hitboxes.h) with real death/respawn tied into the round.
enum class BotState { Idle, Chase, Attack };

struct Bot {
    Vec3 origin{};   // feet position, same convention as camera.x/y/z
    float yaw = 0.0f; // degrees, same convention as Camera::yaw
    float velocityZ = 0.0f;
    bool grounded = true;
    int health = kMaxHealth;
    bool alive = true;
    Team team = Team::T;
    BotState state = BotState::Idle;
    float fireCooldown = 0.0f;
    Vec3 wanderTarget{};
    float wanderTimer = 0.0f; // seconds until a new wander target is picked
    bool moving = false;       // drives idle-vs-run animation selection
    float animTime = 0.0f;
};

// A bot fired at the player this update, for the caller to hook up
// sound/particle feedback without BotSystem needing to know about any of
// the render/audio systems. (Bot death is reported directly by damage()'s
// return value at the call site instead — it happens in response to the
// player's own hitscan, not inside update().)
struct BotFiredEvent {
    Vec3 position{};
};

class BotSystem {
public:
    std::vector<Bot> bots;

    // Spawns `count` bots on `team` at that team's spawn points (falling
    // back to any spawn point if the team has none, same rule
    // pickSpawnForTeam uses for the player).
    void spawn(int count, Team team, const EntitySystem& entities);

    // Heals/respawns every bot at a fresh spawn point — called at the start
    // of each new round, same as the player's own respawn.
    void respawnAll(const EntitySystem& entities);

    int aliveCount() const;

    // Advances every bot one frame: perception (line-of-sight + distance to
    // the player), state (Idle wander / Chase / Attack), movement (wall-slide
    // collision against both static world geometry and brush entities, plus
    // simple gravity/ground-probe), and firing at the player when in range
    // and unobstructed — which applies real damage via damagePlayer().
    // Returns this update's fire events (one per bot that took a shot).
    std::vector<BotFiredEvent> update(float dt, const BspMap& map, const BrushEntitySystem& brushEntities,
                                       const EntitySystem& entities, PlayerState& player,
                                       Vec3 playerEye, Vec3 playerFeet);

    // Applies damage to one bot; returns true if this killed it. Used by
    // the player's own hitscan (see main.cpp's shoot handler).
    bool damage(size_t botIndex, int amount);
};

// Transforms a model's posed-but-untransformed hitboxes (the space
// MdlModel::pose()'s triangles come back in) into true world space by the
// same rigid transform the renderer applies when drawing that model at a
// world position/yaw: rotate around Z by yawDegrees, then translate by
// origin — matching main.cpp's third-person body draw (glTranslatef then
// glRotatef) and Bot's own (origin, yaw) fields exactly.
std::vector<WorldHitbox> transformHitboxesToWorld(const std::vector<WorldHitbox>& localHitboxes,
                                                   Vec3 origin, float yawDegrees);
