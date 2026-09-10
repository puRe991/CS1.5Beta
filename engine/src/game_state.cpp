#include "game_state.h"

#include <algorithm>
#include <cstdlib>

void pickSpawnForTeam(const EntitySystem& entities, Team team, Vec3& outOrigin, float& outYaw) {
    std::vector<const SpawnPoint*> teamSpawns;
    for (const auto& sp : entities.spawns) {
        if (sp.team == team) teamSpawns.push_back(&sp);
    }
    const SpawnPoint* chosen = nullptr;
    if (!teamSpawns.empty()) {
        chosen = teamSpawns[std::rand() % teamSpawns.size()];
    } else if (!entities.spawns.empty()) {
        chosen = &entities.spawns[std::rand() % entities.spawns.size()];
    }
    if (chosen) {
        outOrigin = chosen->origin;
        outYaw = chosen->yaw;
    }
}

void damagePlayer(PlayerState& player, int amount) {
    if (!player.alive) return;
    player.health -= amount;
    if (player.health <= 0) {
        player.health = 0;
        player.alive = false;
    }
}

void endRound(RoundState& round, const std::string& reason) {
    round.phase = RoundPhase::Intermission;
    round.intermissionRemaining = kIntermissionDuration;
    round.endReason = reason;
}

bool updateRound(RoundState& round, PlayerState& player, const EntitySystem& entities,
                  float dt, Vec3& outRespawnOrigin, float& outRespawnYaw) {
    if (round.phase == RoundPhase::Live) {
        if (round.bombPlanted) {
            // Once planted, the bomb clock is the real win condition — a
            // dead player (nobody left to defuse) just lets it run out,
            // no special-casing needed for that beyond skipping the
            // death-ends-round check below.
            round.bombTimer -= dt;
            if (round.bombTimer <= 0.0f) {
                round.bombTimer = 0.0f;
                endRound(round, "BOMB_EXPLODED");
                return false;
            }
        } else {
            round.timeRemaining -= dt;
            if (round.timeRemaining <= 0.0f) {
                round.timeRemaining = 0.0f;
                endRound(round, "TIME");
                return false;
            }
        }

        if (!player.alive && !round.bombPlanted) {
            endRound(round, "DEATH");
        }
        return false;
    }

    // Intermission: count down, then respawn and start the next round.
    round.intermissionRemaining -= dt;
    if (round.intermissionRemaining > 0.0f) return false;

    player.health = kMaxHealth;
    player.alive = true;
    player.money = std::min(kMaxMoney, player.money + kRoundMoneyReward);
    round.phase = RoundPhase::Live;
    round.timeRemaining = kRoundDuration;
    round.roundNumber += 1;
    round.endReason.clear();
    round.bombPlanted = false;
    round.bombTimer = 0.0f;
    round.plantProgress = 0.0f;
    round.defuseProgress = 0.0f;

    pickSpawnForTeam(entities, player.team, outRespawnOrigin, outRespawnYaw);
    return true;
}
