#include "bots.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "weapons.h"

namespace {

constexpr float kBotHalfWidth = 16.0f;   // matches the player's own hull half-width
constexpr float kBotHeight = 72.0f;      // matches the player's standing hull height
constexpr int kBotHull = 1;              // standing player hull, same as the human player uses
constexpr float kBotMoveSpeed = 200.0f;  // units/sec — a little slower than the player's 250
constexpr float kBotGravity = 800.0f;    // matches main.cpp's kGravity
constexpr float kBotViewDistance = 2048.0f; // max distance a bot can perceive the player at all
constexpr float kBotEngageRange = 1400.0f;  // max distance a bot will actually shoot from
constexpr float kBotStopDistance = 200.0f;  // stop closing once this near — keep some space, not melee-rush
constexpr float kWanderRadius = 400.0f;     // how far a wander target is picked from the bot's current spot
constexpr float kWanderInterval = 4.0f;     // seconds between picking a new wander target on reaching/stalling
// Fixed weapon profile every bot uses — the AK47 entry, same "index 16 is
// the AK47" convention main.cpp's own default loadout already relies on.
// No per-bot loadout/buy logic yet (see the README's Bots & AI TODO).
constexpr int kBotWeaponIndex = 16;
constexpr float kBotHitChance = 0.55f; // simple stand-in for real aim/accuracy modeling

Vec3 randomPointNear(Vec3 center, float radius) {
    float angle = ((float)std::rand() / (float)RAND_MAX) * 6.2831853f;
    float dist = ((float)std::rand() / (float)RAND_MAX) * radius;
    return Vec3{center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist, center.z};
}

// Resolves one bot's movement toward `target` this frame against the same
// wall-slide + ground-probe rules main.cpp's player movement uses: per-axis
// horizontal resolution (so it slides along a wall instead of stopping
// dead), then a downward probe for gravity/ground snap. No acceleration
// curve or air control — bots don't jump, so a direct kinematic step is
// enough and keeps this simple.
void stepBotMovement(Bot& bot, Vec3 target, float dt, const BspMap& map, const BrushEntitySystem& brushEntities) {
    float dx = target.x - bot.origin.x, dy = target.y - bot.origin.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist > 1.0f) {
        bot.yaw = std::atan2(dy, dx) * 180.0f / 3.14159265f;
        float moveX = (dx / dist) * kBotMoveSpeed * dt;
        float moveY = (dy / dist) * kBotMoveSpeed * dt;
        // Don't overshoot a near target in one step.
        if (std::sqrt(moveX * moveX + moveY * moveY) > dist) {
            moveX = dx; moveY = dy;
        }

        Vec3 candidate = bot.origin;
        candidate.x += moveX;
        if (map.pointInSolidHull(candidate, kBotHull) || brushEntities.pointInSolid(map, candidate, kBotHull)) {
            candidate.x = bot.origin.x;
        }
        candidate.y += moveY;
        if (map.pointInSolidHull(candidate, kBotHull) || brushEntities.pointInSolid(map, candidate, kBotHull)) {
            candidate.y = bot.origin.y;
        }
        bot.moving = candidate.x != bot.origin.x || candidate.y != bot.origin.y;
        bot.origin.x = candidate.x;
        bot.origin.y = candidate.y;
    } else {
        bot.moving = false;
    }

    Vec3 groundProbe = bot.origin;
    groundProbe.z -= 2.0f;
    bot.grounded = map.pointInSolidHull(groundProbe, kBotHull) || brushEntities.pointInSolid(map, groundProbe, kBotHull);
    if (bot.grounded) {
        bot.velocityZ = 0.0f;
    } else {
        bot.velocityZ -= kBotGravity * dt;
        Vec3 fallCandidate = bot.origin;
        fallCandidate.z += bot.velocityZ * dt;
        if (!map.pointInSolidHull(fallCandidate, kBotHull) && !brushEntities.pointInSolid(map, fallCandidate, kBotHull)) {
            bot.origin.z = fallCandidate.z;
        }
    }
}

} // namespace

void BotSystem::spawn(int count, Team team, const EntitySystem& entities) {
    bots.clear();
    bots.reserve(count);
    for (int i = 0; i < count; ++i) {
        Bot b;
        b.team = team;
        Vec3 origin;
        float yaw = 0.0f;
        pickSpawnForTeam(entities, team, origin, yaw);
        b.origin = origin;
        b.yaw = yaw;
        b.wanderTarget = origin;
        bots.push_back(b);
    }
}

void BotSystem::respawnAll(const EntitySystem& entities) {
    for (Bot& b : bots) {
        Vec3 origin;
        float yaw = 0.0f;
        pickSpawnForTeam(entities, b.team, origin, yaw);
        b.origin = origin;
        b.yaw = yaw;
        b.health = kMaxHealth;
        b.alive = true;
        b.state = BotState::Idle;
        b.fireCooldown = 0.0f;
        b.wanderTarget = origin;
        b.wanderTimer = 0.0f;
    }
}

int BotSystem::aliveCount() const {
    int n = 0;
    for (const Bot& b : bots) if (b.alive) ++n;
    return n;
}

bool BotSystem::damage(size_t botIndex, int amount) {
    if (botIndex >= bots.size()) return false;
    Bot& b = bots[botIndex];
    if (!b.alive) return false;
    b.health -= amount;
    if (b.health <= 0) {
        b.health = 0;
        b.alive = false;
        return true;
    }
    return false;
}

std::vector<BotFiredEvent> BotSystem::update(float dt, const BspMap& map, const BrushEntitySystem& brushEntities,
                                              const EntitySystem& entities, PlayerState& player,
                                              Vec3 playerEye, Vec3 playerFeet) {
    std::vector<BotFiredEvent> events;
    const WeaponDef& weapon = weaponByIndex(kBotWeaponIndex);

    for (Bot& bot : bots) {
        if (!bot.alive) continue;
        bot.animTime += dt;
        if (bot.fireCooldown > 0.0f) bot.fireCooldown -= dt;

        Vec3 botEye = bot.origin;
        botEye.z += kBotHeight * 0.9f; // roughly eye height on the standing hull

        bool canSeePlayer = false;
        float distToPlayer = 1e30f;
        if (player.alive) {
            float dx = playerEye.x - botEye.x, dy = playerEye.y - botEye.y, dz = playerEye.z - botEye.z;
            distToPlayer = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distToPlayer <= kBotViewDistance) {
                Vec3 hit;
                bool blocked = map.traceLine(botEye, playerEye, hit, nullptr);
                if (blocked) {
                    float hdx = hit.x - botEye.x, hdy = hit.y - botEye.y, hdz = hit.z - botEye.z;
                    float hitDist = std::sqrt(hdx * hdx + hdy * hdy + hdz * hdz);
                    canSeePlayer = hitDist >= distToPlayer - 8.0f; // trace reached (near) the player, nothing in the way
                } else {
                    canSeePlayer = true; // trace never went solid at all
                }
            }
        }

        if (canSeePlayer) {
            bot.state = distToPlayer <= kBotEngageRange ? BotState::Attack : BotState::Chase;
        } else {
            bot.state = BotState::Idle;
        }

        switch (bot.state) {
            case BotState::Attack: {
                Vec3 moveTarget = playerFeet;
                if (distToPlayer <= kBotStopDistance) moveTarget = bot.origin; // hold position, keep firing
                stepBotMovement(bot, moveTarget, dt, map, brushEntities);
                bot.yaw = std::atan2(playerEye.y - botEye.y, playerEye.x - botEye.x) * 180.0f / 3.14159265f;

                if (bot.fireCooldown <= 0.0f) {
                    bot.fireCooldown = 60.0f / weapon.fireRateRpm;
                    events.push_back(BotFiredEvent{botEye});
                    // Deliberately simple stand-in for real accuracy/spread
                    // modeling (see the weapon-system-depth README TODO): a
                    // flat hit-chance roll instead of an actual cone of fire.
                    if (((float)std::rand() / (float)RAND_MAX) < kBotHitChance) {
                        damagePlayer(player, weapon.damage);
                    }
                }
                break;
            }
            case BotState::Chase:
                stepBotMovement(bot, playerFeet, dt, map, brushEntities);
                break;
            case BotState::Idle: {
                bot.wanderTimer -= dt;
                float dx = bot.wanderTarget.x - bot.origin.x, dy = bot.wanderTarget.y - bot.origin.y;
                bool reached = (dx * dx + dy * dy) < (32.0f * 32.0f);
                if (bot.wanderTimer <= 0.0f || reached) {
                    bot.wanderTarget = randomPointNear(bot.origin, kWanderRadius);
                    bot.wanderTimer = kWanderInterval;
                }
                stepBotMovement(bot, bot.wanderTarget, dt, map, brushEntities);
                break;
            }
        }
    }

    (void)entities; // reserved for future patrol-route/spawn-aware wandering
    return events;
}

std::vector<WorldHitbox> transformHitboxesToWorld(const std::vector<WorldHitbox>& localHitboxes,
                                                   Vec3 origin, float yawDegrees) {
    std::vector<WorldHitbox> out;
    out.reserve(localHitboxes.size());

    float yawRad = yawDegrees * 3.14159265f / 180.0f;
    float c = std::cos(yawRad), s = std::sin(yawRad);

    for (const WorldHitbox& local : localHitboxes) {
        float wmin[3] = {1e30f, 1e30f, 1e30f};
        float wmax[3] = {-1e30f, -1e30f, -1e30f};
        for (int i = 0; i < 8; ++i) {
            float lx = (i & 1) ? local.maxs[0] : local.mins[0];
            float ly = (i & 2) ? local.maxs[1] : local.mins[1];
            float lz = (i & 4) ? local.maxs[2] : local.mins[2];
            // Rotate around Z, then translate — matches the renderer's
            // glTranslatef(origin) * glRotatef(yaw, Z) draw transform.
            float wx = lx * c - ly * s + origin.x;
            float wy = lx * s + ly * c + origin.y;
            float wz = lz + origin.z;
            wmin[0] = std::min(wmin[0], wx); wmax[0] = std::max(wmax[0], wx);
            wmin[1] = std::min(wmin[1], wy); wmax[1] = std::max(wmax[1], wy);
            wmin[2] = std::min(wmin[2], wz); wmax[2] = std::max(wmax[2], wz);
        }
        WorldHitbox world;
        world.bone = local.bone;
        world.part = local.part;
        world.mins[0] = wmin[0]; world.mins[1] = wmin[1]; world.mins[2] = wmin[2];
        world.maxs[0] = wmax[0]; world.maxs[1] = wmax[1]; world.maxs[2] = wmax[2];
        out.push_back(world);
    }
    return out;
}
