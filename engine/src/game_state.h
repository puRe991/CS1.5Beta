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
constexpr float kBombTimerDuration = 35.0f;    // seconds from plant to detonation
constexpr float kPlantDuration = 3.0f;         // seconds holding E in a bomb zone to plant
constexpr float kDefuseDuration = 5.0f;        // seconds holding E near the bomb to defuse
constexpr float kDefuseRadius = 80.0f;         // units — must be this close to the bomb to defuse

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
    // "TIME" (CT win), "DEATH", "BOMB_EXPLODED" (T win), "BOMB_DEFUSED" (CT
    // win) — set when phase == Intermission.
    std::string endReason;

    bool bombPlanted = false;
    float bombTimer = 0.0f;   // counts down from kBombTimerDuration once planted
    Vec3 bombPosition{};
    float plantProgress = 0.0f;  // seconds of continuous plant-key hold so far
    float defuseProgress = 0.0f; // seconds of continuous defuse-key hold so far

    int ctScore = 0;
    int tScore = 0;
};

// Ends the round immediately with the given reason, starting the
// intermission/respawn countdown, and credits the winning side's score
// (TIME/BOMB_DEFUSED -> CT, BOMB_EXPLODED -> T, DEATH -> whichever team the
// player *isn't* on, since they were the one eliminated). Exposed so
// main.cpp's plant/defuse interaction (which owns the input/zone-distance
// checks) can end a round the same way updateRound() does internally for
// timeout/death/explosion.
void endRound(RoundState& round, const PlayerState& player, const std::string& reason);

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
