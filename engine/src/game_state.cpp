#include "game_state.h"

#include <algorithm>
#include <cstdlib>

void damagePlayer(PlayerState& player, int amount) {
    if (!player.alive) return;
    player.health -= amount;
    if (player.health <= 0) {
        player.health = 0;
        player.alive = false;
    }
}

bool updateRound(RoundState& round, PlayerState& player, const EntitySystem& entities,
                  float dt, Vec3& outRespawnOrigin, float& outRespawnYaw) {
    if (round.phase == RoundPhase::Live) {
        round.timeRemaining -= dt;
        if (!player.alive) {
            round.phase = RoundPhase::Intermission;
            round.intermissionRemaining = kIntermissionDuration;
            round.endReason = "DEATH";
        } else if (round.timeRemaining <= 0.0f) {
            round.timeRemaining = 0.0f;
            round.phase = RoundPhase::Intermission;
            round.intermissionRemaining = kIntermissionDuration;
            round.endReason = "TIME";
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

    // Prefer a CT spawn (matches the rest of the engine's spawn choice
    // convention); fall back to any spawn point.
    std::vector<const SpawnPoint*> ctSpawns;
    for (const auto& sp : entities.spawns) {
        if (sp.team == Team::CT) ctSpawns.push_back(&sp);
    }
    const SpawnPoint* chosen = nullptr;
    if (!ctSpawns.empty()) {
        chosen = ctSpawns[std::rand() % ctSpawns.size()];
    } else if (!entities.spawns.empty()) {
        chosen = &entities.spawns[std::rand() % entities.spawns.size()];
    }
    if (chosen) {
        outRespawnOrigin = chosen->origin;
        outRespawnYaw = chosen->yaw;
    }
    return true;
}
