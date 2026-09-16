#pragma once

#include <vector>

#include "assets/bsp.h"
#include "assets/mdl.h"
#include "brush_entities.h"
#include "entities.h"
#include "game_state.h"
#include "nav.h"

// A real AI opponent, not a stub — see the README's Bots & AI section for
// the honest list of what this still doesn't cover (no real per-round buy
// strategy beyond "spend on the best affordable gun"; no utility usage,
// since bots don't have grenades — none exist yet; no explicit team chat,
// just the shared last-seen-position callout below).
enum class BotState { Idle, Search, Chase, Attack };

// One difficulty tier's tuning: how long a bot takes to react once it spots
// the player, how often its shots actually connect, how far it can see,
// and how far a gunshot alerts it even without line of sight.
enum class BotDifficulty { Beginner, Easy, Normal, Expert };

struct BotDifficultyTuning {
    float reactionTime;
    float hitChance;
    float viewDistance;
    float hearingRadius;
};

BotDifficultyTuning tuningForDifficulty(BotDifficulty difficulty);

struct Bot {
    Vec3 origin{};   // feet position, same convention as camera.x/y/z
    float yaw = 0.0f; // degrees, same convention as Camera::yaw
    float velocityZ = 0.0f;
    bool grounded = true;
    int health = kMaxHealth;
    bool alive = true;
    Team team = Team::T;
    BotDifficulty difficulty = BotDifficulty::Normal;
    BotState state = BotState::Idle;
    float fireCooldown = 0.0f;
    float reactionRemaining = 0.0f; // must reach 0 before a freshly-spotted bot will actually fire

    Vec3 wanderTarget{};
    float wanderTimer = 0.0f; // seconds until a new wander/patrol target is picked

    Vec3 investigateTarget{}; // where Search state heads: a heard sound or a teammate's last-seen player position
    float investigateTimer = 0.0f; // gives up and returns to Idle once this runs out

    // Coarse path-following state (see NavGraph in nav.h): recomputed
    // whenever the movement goal moves far enough from what the current
    // path was planned for.
    std::vector<Vec3> path;
    size_t pathIndex = 0;
    Vec3 pathGoal{};

    int money = kStartingMoney;
    int weaponIndex = 16; // AK47 — the default before a bot's first buy

    // Index into EntitySystem::bombTargets this bot currently cares about:
    // for T, the site it's pushing to plant; for CT, the site it's
    // defending. Round-robin assigned across bombTargets so bots spread
    // out instead of stacking one site, and reassigned ("rotating") once
    // the bomb is actually planted — see assignBombSites()/BotSystem::update()
    // in bots.cpp. -1 if the map has no bomb sites at all.
    int assignedSite = -1;
    float actionProgress = 0.0f; // seconds spent continuously planting/defusing, own timer, separate from the player's

    bool moving = false; // drives idle-vs-run animation selection
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
    // pickSpawnForTeam uses for the player), all at the given difficulty,
    // and has each one spend its starting money on a weapon immediately
    // (see buyWeapon() in bots.cpp).
    void spawn(int count, Team team, const EntitySystem& entities, BotDifficulty difficulty = BotDifficulty::Normal);

    // Heals/respawns every bot at a fresh spawn point, credits this round's
    // money reward, and has each one re-buy a weapon — called at the start
    // of each new round, same as the player's own respawn.
    void respawnAll(const EntitySystem& entities);

    int aliveCount() const;

    // Builds (or rebuilds) the coarse waypoint graph bots path-find over.
    // Call once after the map/entities are loaded (mapMins/mapMaxs is the
    // worldspawn model's bounds, same as main.cpp already computes for the
    // radar). A BotSystem that never calls this just falls back to
    // direct-line steering for movement, same as before this existed.
    void buildNav(const BspMap& map, const EntitySystem& entities, Vec3 mapMins, Vec3 mapMaxs);

    // Advances every bot one frame: perception (line-of-sight + distance,
    // plus hearing — `externalSounds` is this frame's outside noises, e.g.
    // the player's own gunfire, reported by the caller), state (Idle wander
    // / Search / Chase / Attack), movement (nav-graph path-following where
    // a graph exists, wall-slide collision either way), and firing at the
    // player once in range, unobstructed, and past its reaction delay —
    // which applies real damage via damagePlayer(). Returns this update's
    // fire events (one per bot that took a shot); bots hear each other's
    // gunfire too; a teammate who has the player in sight shares that
    // position with the rest of the team for a few seconds even after
    // losing sight of it themself.
    //
    // Also drives site tactics: a T bot standing in its assigned bomb site
    // (and not busy fighting) plants it, a CT bot within defuse range of a
    // planted bomb (ditto) defuses it — both using their own actionProgress
    // timer, entirely separate from the player's own round.plantProgress/
    // defuseProgress, so the two never double-count. CT bots reassign
    // ("rotate") to whichever site the bomb actually got planted at.
    std::vector<BotFiredEvent> update(float dt, const BspMap& map, const BrushEntitySystem& brushEntities,
                                       const EntitySystem& entities, PlayerState& player, RoundState& round,
                                       Vec3 playerEye, Vec3 playerFeet,
                                       const std::vector<Vec3>& externalSounds = {});

    // Applies damage to one bot; returns true if this killed it. Used by
    // the player's own hitscan (see main.cpp's shoot handler).
    bool damage(size_t botIndex, int amount);

private:
    NavGraph nav_;
    Vec3 teamLastKnownEnemyPos_{};
    float teamLastKnownEnemyAge_ = 1e9f; // seconds since any teammate last saw the player; huge = "never"
};

// Transforms a model's posed-but-untransformed hitboxes (the space
// MdlModel::pose()'s triangles come back in) into true world space by the
// same rigid transform the renderer applies when drawing that model at a
// world position/yaw: rotate around Z by yawDegrees, then translate by
// origin — matching main.cpp's third-person body draw (glTranslatef then
// glRotatef) and Bot's own (origin, yaw) fields exactly.
std::vector<WorldHitbox> transformHitboxesToWorld(const std::vector<WorldHitbox>& localHitboxes,
                                                   Vec3 origin, float yawDegrees);
