#pragma once

#include <string>

#include "entities.h"
#include "weapons.h"

// First real gameplay loop: player health/death/respawn and a round timer.
// Deliberately scoped for single-player testing (no bots/opponents yet):
// damage currently comes from fall damage and a debug test key, and a
// round "ends" on either the timer running out or the player dying — real
// win conditions (bomb, elimination, hostages) need actual opposing
// entities/AI, tracked separately in the README TODO.

constexpr int kMaxHealth = 100;
constexpr float kRoundDuration = 115.0f;       // seconds, ~classic CS round length
constexpr float kIntermissionDuration = 5.0f;  // seconds between rounds

struct PlayerState {
    int health = kMaxHealth;
    bool alive = true;
    int money = kStartingMoney;
    Team team = Team::CT;
};

// Picks a spawn point for `team` (falling back to any spawn, then leaving
// origin/yaw untouched if the map has none at all). Shared by the initial
// spawn and every respawn so both use the same team-aware selection.
void pickSpawnForTeam(const EntitySystem& entities, Team team, Vec3& outOrigin, float& outYaw);

enum class RoundPhase { Live, Intermission };

struct RoundState {
    RoundPhase phase = RoundPhase::Live;
    float timeRemaining = kRoundDuration;
    float intermissionRemaining = 0.0f;
    int roundNumber = 1;
    std::string endReason; // "TIME" or "DEATH", set when phase == Intermission
};

// Applies damage; if it kills the player, marks them dead (caller is
// responsible for triggering the round-end/respawn flow on that transition).
void damagePlayer(PlayerState& player, int amount);

// Advances the round clock. When Live and time runs out, or the player is
// not alive, ends the round (sets Intermission + endReason). When the
// intermission timer elapses, respawns the player at a random CT spawn
// and starts a new round. Returns true the frame a respawn happens (so the
// caller can reset ammo/etc alongside it).
bool updateRound(RoundState& round, PlayerState& player, const EntitySystem& entities,
                  float dt, Vec3& outRespawnOrigin, float& outRespawnYaw);
