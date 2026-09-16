#include "bots.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "weapons.h"

namespace {

constexpr float kBotHeight = 72.0f;      // matches the player's standing hull height
constexpr int kBotHull = 1;              // standing player hull, same as the human player uses
constexpr float kBotMoveSpeed = 200.0f;  // units/sec — a little slower than the player's 250
constexpr float kBotGravity = 800.0f;    // matches main.cpp's kGravity
constexpr float kBotEngageRange = 1400.0f;  // max distance a bot will actually shoot from
constexpr float kBotStopDistance = 200.0f;  // stop closing once this near — keep some space, not melee-rush
constexpr float kWanderRadius = 400.0f;     // how far a random patrol point is picked, when there's no objective to head for
constexpr float kWanderInterval = 4.0f;     // seconds between picking a new wander/patrol target on reaching/stalling
constexpr float kDefendRadius = 300.0f;     // how far out a CT holds from its assigned site's center while defending
constexpr float kSearchDuration = 6.0f;     // seconds a bot investigates a sound/callout before giving up
constexpr float kCalloutFreshness = 5.0f;   // a teammate's last-seen player position is usable for this long
constexpr float kRepathThreshold = 150.0f;  // goal moved this far from the last plan: recompute the path
constexpr float kWaypointRadius = 48.0f;    // "close enough" to a path waypoint to advance to the next one
constexpr float kZoneInset = 16.0f;         // keep planting/defending targets this far off a zone's own walls

Vec3 randomPointNear(Vec3 center, float radius) {
    float angle = ((float)std::rand() / (float)RAND_MAX) * 6.2831853f;
    float dist = ((float)std::rand() / (float)RAND_MAX) * radius;
    return Vec3{center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist, center.z};
}

Vec3 zoneCenter(const ZoneRegion& zone) {
    return Vec3{(zone.mins.x + zone.maxs.x) * 0.5f, (zone.mins.y + zone.maxs.y) * 0.5f, (zone.mins.z + zone.maxs.z) * 0.5f};
}

// A random point actually inside `zone`'s bounds (inset a little from its
// own walls), for a T bot to walk to and hold while planting — unlike a
// random point *near* the center, this reliably lands inside the zone so
// pointInZone() (and therefore the plant check) stays true once it arrives.
Vec3 pointInsideZone(const ZoneRegion& zone) {
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    auto rand01 = []() { return (float)std::rand() / (float)RAND_MAX; };
    float minX = zone.mins.x, maxX = zone.maxs.x;
    float minY = zone.mins.y, maxY = zone.maxs.y;
    if (maxX - minX > kZoneInset * 2.0f) { minX += kZoneInset; maxX -= kZoneInset; }
    if (maxY - minY > kZoneInset * 2.0f) { minY += kZoneInset; maxY -= kZoneInset; }
    return Vec3{lerp(minX, maxX, rand01()), lerp(minY, maxY, rand01()), zone.mins.z};
}

// A point roughly `radius` out from `center` (a ring, not a filled disk —
// biased toward the outer edge) for a CT bot to hold while watching a site
// from a bit of a distance, rather than standing in the middle of it.
Vec3 defendOrbitPoint(Vec3 center, float radius) {
    float angle = ((float)std::rand() / (float)RAND_MAX) * 6.2831853f;
    float dist = radius * (0.7f + 0.3f * ((float)std::rand() / (float)RAND_MAX));
    return Vec3{center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist, center.z};
}

// Index into `zones` whose center is nearest `pos` — used to have CT bots
// rotate to whichever site the bomb actually got planted at.
int nearestZoneIndex(Vec3 pos, const std::vector<ZoneRegion>& zones) {
    int best = -1;
    float bestDist = 1e30f;
    for (size_t i = 0; i < zones.size(); ++i) {
        Vec3 c = zoneCenter(zones[i]);
        float dx = c.x - pos.x, dy = c.y - pos.y;
        float d = dx * dx + dy * dy;
        if (d < bestDist) { bestDist = d; best = (int)i; }
    }
    return best;
}

// Spreads bots round-robin across the map's bomb sites (so T bots split up
// to push multiple sites, and CT bots don't all stack the same one) instead
// of every bot independently picking at random each time it wanders.
void assignBombSites(std::vector<Bot>& bots, const EntitySystem& entities) {
    if (entities.bombTargets.empty()) {
        for (Bot& b : bots) b.assignedSite = -1;
        return;
    }
    int n = (int)entities.bombTargets.size();
    int i = 0;
    for (Bot& b : bots) { b.assignedSite = i % n; ++i; }
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

// Picks the next point a bot should steer toward on its way to `goal`,
// using the bot's cached path (recomputing it from the nav graph when the
// goal has moved far enough, or there's no path yet). Falls back to
// steering straight at `goal` when there's no graph, or the graph can't
// reach it — exactly the old direct-line behavior, so a bot never just
// stops because pathfinding failed.
Vec3 nextPathTarget(Bot& bot, const NavGraph& nav, Vec3 goal) {
    if (nav.empty()) return goal;

    float gdx = goal.x - bot.pathGoal.x, gdy = goal.y - bot.pathGoal.y;
    bool needsPath = bot.path.empty() || bot.pathIndex >= bot.path.size() ||
                      std::sqrt(gdx * gdx + gdy * gdy) > kRepathThreshold;
    if (needsPath) {
        bot.path = nav.findPath(bot.origin, goal);
        bot.pathIndex = 0;
        bot.pathGoal = goal;
    }
    if (bot.path.empty()) return goal;

    Vec3 waypoint = bot.path[bot.pathIndex];
    float wdx = waypoint.x - bot.origin.x, wdy = waypoint.y - bot.origin.y;
    if (std::sqrt(wdx * wdx + wdy * wdy) < kWaypointRadius && bot.pathIndex + 1 < bot.path.size()) {
        ++bot.pathIndex;
    }
    return bot.path[bot.pathIndex];
}

// Spends `bot.money` on the priciest weapon it can afford, preferring a
// primary (rifle/SMG/shotgun/sniper/heavy) over a pistol — a simple "buy
// the best gun you can" heuristic, not a real buy strategy (no eco/force-
// buy/save decisions, no armor, no utility purchases; see the README).
void buyWeapon(Bot& bot) {
    auto isPrimary = [](WeaponCategory c) {
        return c == WeaponCategory::Rifle || c == WeaponCategory::Smg ||
               c == WeaponCategory::Shotgun || c == WeaponCategory::Sniper || c == WeaponCategory::Heavy;
    };

    int bestIndex = -1, bestPrice = -1;
    for (int i = 0; i < kWeaponCatalogCount; ++i) {
        const WeaponDef& w = kWeaponCatalog[i];
        if (!isPrimary(w.category)) continue;
        if (w.price <= bot.money && w.price > bestPrice) { bestPrice = w.price; bestIndex = i; }
    }
    if (bestIndex < 0) {
        for (int i = 0; i < kWeaponCatalogCount; ++i) {
            const WeaponDef& w = kWeaponCatalog[i];
            if (w.category != WeaponCategory::Pistol) continue;
            if (w.price <= bot.money && w.price > bestPrice) { bestPrice = w.price; bestIndex = i; }
        }
    }
    if (bestIndex >= 0) {
        bot.money -= bestPrice;
        bot.weaponIndex = bestIndex;
    }
}

} // namespace

BotDifficultyTuning tuningForDifficulty(BotDifficulty difficulty) {
    switch (difficulty) {
        case BotDifficulty::Beginner: return {1.2f, 0.30f, 1400.0f, 500.0f};
        case BotDifficulty::Easy:     return {0.8f, 0.42f, 1700.0f, 700.0f};
        case BotDifficulty::Expert:   return {0.15f, 0.75f, 2400.0f, 1100.0f};
        case BotDifficulty::Normal:
        default:                     return {0.4f, 0.55f, 2048.0f, 900.0f};
    }
}

void BotSystem::spawn(int count, Team team, const EntitySystem& entities, BotDifficulty difficulty) {
    bots.clear();
    bots.reserve(count);
    for (int i = 0; i < count; ++i) {
        Bot b;
        b.team = team;
        b.difficulty = difficulty;
        Vec3 origin;
        float yaw = 0.0f;
        pickSpawnForTeam(entities, team, origin, yaw);
        b.origin = origin;
        b.yaw = yaw;
        b.wanderTarget = origin;
        buyWeapon(b);
        bots.push_back(b);
    }
    assignBombSites(bots, entities);
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
        b.reactionRemaining = 0.0f;
        b.wanderTarget = origin;
        b.wanderTimer = 0.0f;
        b.investigateTimer = 0.0f;
        b.path.clear();
        b.pathIndex = 0;
        b.money = std::min(kMaxMoney, b.money + kRoundMoneyReward);
        buyWeapon(b);
        b.actionProgress = 0.0f;
    }
    assignBombSites(bots, entities);
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

void BotSystem::buildNav(const BspMap& map, const EntitySystem& entities, Vec3 mapMins, Vec3 mapMaxs) {
    std::vector<Vec3> seeds;
    for (const SpawnPoint& sp : entities.spawns) seeds.push_back(sp.origin);
    for (const ZoneRegion& z : entities.bombTargets) seeds.push_back(zoneCenter(z));
    for (const ZoneRegion& z : entities.buyZones) seeds.push_back(zoneCenter(z));
    nav_.build(map, seeds, mapMins, mapMaxs);
}

std::vector<BotFiredEvent> BotSystem::update(float dt, const BspMap& map, const BrushEntitySystem& brushEntities,
                                              const EntitySystem& entities, PlayerState& player, RoundState& round,
                                              Vec3 playerEye, Vec3 playerFeet,
                                              const std::vector<Vec3>& externalSounds) {
    std::vector<BotFiredEvent> events;
    std::vector<Vec3> soundsThisFrame = externalSounds;
    std::vector<bool> sawPlayer(bots.size(), false);

    for (size_t bi = 0; bi < bots.size(); ++bi) {
        Bot& bot = bots[bi];
        if (!bot.alive) continue;
        bot.animTime += dt;
        if (bot.fireCooldown > 0.0f) bot.fireCooldown -= dt;
        if (bot.reactionRemaining > 0.0f) bot.reactionRemaining -= dt;

        const BotDifficultyTuning tuning = tuningForDifficulty(bot.difficulty);
        const WeaponDef& weapon = weaponByIndex(bot.weaponIndex);

        Vec3 botEye = bot.origin;
        botEye.z += kBotHeight * 0.9f; // roughly eye height on the standing hull

        bool canSeePlayer = false;
        float distToPlayer = 1e30f;
        if (player.alive) {
            float dx = playerEye.x - botEye.x, dy = playerEye.y - botEye.y, dz = playerEye.z - botEye.z;
            distToPlayer = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distToPlayer <= tuning.viewDistance) {
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
        if (canSeePlayer) sawPlayer[bi] = true;

        BotState newState;
        if (canSeePlayer) {
            newState = distToPlayer <= kBotEngageRange ? BotState::Attack : BotState::Chase;
        } else if (bot.state == BotState::Search && bot.investigateTimer > 0.0f) {
            newState = BotState::Search;
        } else {
            newState = BotState::Idle;
        }
        // A fresh Attack acquisition starts the reaction-time clock; staying
        // in Attack across frames (the common case) doesn't reset it, or a
        // bot would never actually finish reacting and fire.
        bool enteringAttack = newState == BotState::Attack && bot.state != BotState::Attack;
        bot.state = newState;
        if (enteringAttack) bot.reactionRemaining = tuning.reactionTime;

        switch (bot.state) {
            case BotState::Attack: {
                Vec3 moveTarget = distToPlayer <= kBotStopDistance ? bot.origin : playerFeet; // hold position once close, keep firing
                stepBotMovement(bot, moveTarget, dt, map, brushEntities);
                bot.yaw = std::atan2(playerEye.y - botEye.y, playerEye.x - botEye.x) * 180.0f / 3.14159265f;

                if (bot.fireCooldown <= 0.0f && bot.reactionRemaining <= 0.0f) {
                    bot.fireCooldown = 60.0f / weapon.fireRateRpm;
                    events.push_back(BotFiredEvent{botEye});
                    soundsThisFrame.push_back(botEye); // teammates (and the player) can hear this shot
                    // Deliberately simple stand-in for real accuracy/spread
                    // modeling (see the weapon-system-depth README TODO): a
                    // flat hit-chance roll instead of an actual cone of fire.
                    if (((float)std::rand() / (float)RAND_MAX) < tuning.hitChance) {
                        damagePlayer(player, weapon.damage);
                    }
                }
                break;
            }
            case BotState::Chase:
                // Already has line of sight (that's how Chase was entered) —
                // direct pursuit is fine here without routing around anything.
                stepBotMovement(bot, playerFeet, dt, map, brushEntities);
                break;
            case BotState::Search: {
                bot.investigateTimer -= dt;
                Vec3 target = nextPathTarget(bot, nav_, bot.investigateTarget);
                stepBotMovement(bot, target, dt, map, brushEntities);
                break;
            }
            case BotState::Idle: {
                // Role split: T pushes into its assigned site to plant (or,
                // once the bomb is down, holds near wherever it landed); CT
                // holds a perimeter around its assigned site instead of
                // standing in the middle of it, and rotates — reassigns
                // assignedSite — to the actual planted site once there is
                // one, then closes in to defuse range.
                bool hasSite = bot.assignedSite >= 0 && (size_t)bot.assignedSite < entities.bombTargets.size();
                if (bot.team == Team::CT && round.bombPlanted) {
                    int plantedSite = nearestZoneIndex(round.bombPosition, entities.bombTargets);
                    if (plantedSite >= 0) bot.assignedSite = plantedSite;
                    bot.wanderTarget = round.bombPosition; // rush in to defuse range
                    bot.wanderTimer = kWanderInterval;
                } else if (bot.team == Team::T && round.bombPlanted) {
                    bot.wanderTarget = defendOrbitPoint(round.bombPosition, kDefendRadius); // hold the plant against a retake
                    bot.wanderTimer = kWanderInterval;
                } else {
                    bot.wanderTimer -= dt;
                    float dx = bot.wanderTarget.x - bot.origin.x, dy = bot.wanderTarget.y - bot.origin.y;
                    bool reached = (dx * dx + dy * dy) < (32.0f * 32.0f);
                    if (bot.wanderTimer <= 0.0f || reached) {
                        if (hasSite) {
                            const ZoneRegion& zone = entities.bombTargets[bot.assignedSite];
                            bot.wanderTarget = bot.team == Team::T ? pointInsideZone(zone)
                                                                    : defendOrbitPoint(zoneCenter(zone), kDefendRadius);
                        } else {
                            bot.wanderTarget = randomPointNear(bot.origin, kWanderRadius);
                        }
                        bot.wanderTimer = kWanderInterval;
                    }
                }
                Vec3 target = nextPathTarget(bot, nav_, bot.wanderTarget);
                stepBotMovement(bot, target, dt, map, brushEntities);
                break;
            }
        }

        // Bomb plant/defuse: its own progress timer, entirely separate from
        // the player's own round.plantProgress/defuseProgress, so a bot and
        // the player acting at the same time never double-count. Only
        // while not actively fighting — a bot under fire defends itself
        // instead of standing still trying to plant/defuse through it.
        bool busy = bot.state == BotState::Attack;
        if (bot.team == Team::T && !round.bombPlanted && !busy) {
            bool inSite = false;
            for (const ZoneRegion& z : entities.bombTargets) if (pointInZone(z, bot.origin)) { inSite = true; break; }
            if (inSite) {
                bot.actionProgress += dt;
                if (bot.actionProgress >= kPlantDuration) {
                    round.bombPlanted = true;
                    round.bombTimer = kBombTimerDuration;
                    round.bombPosition = bot.origin;
                    bot.actionProgress = 0.0f;
                }
            } else {
                bot.actionProgress = 0.0f;
            }
        } else if (bot.team == Team::CT && round.bombPlanted && !busy) {
            float ddx = bot.origin.x - round.bombPosition.x, ddy = bot.origin.y - round.bombPosition.y,
                  ddz = bot.origin.z - round.bombPosition.z;
            float bombDist = std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz);
            if (bombDist <= kDefuseRadius) {
                bot.actionProgress += dt;
                if (bot.actionProgress >= kDefuseDuration) {
                    endRound(round, player, "BOMB_DEFUSED");
                    bot.actionProgress = 0.0f;
                }
            } else {
                bot.actionProgress = 0.0f;
            }
        } else {
            bot.actionProgress = 0.0f;
        }
    }

    // Team hearing/callouts: any bot that saw the player this frame shares
    // that position with the rest of the team; a purely-Idle bot (nothing
    // else already drawing its attention) investigates the nearest thing
    // it can hear, preferring a fresh sound over stale team intel.
    teamLastKnownEnemyAge_ += dt;
    for (bool saw : sawPlayer) {
        if (saw) { teamLastKnownEnemyPos_ = playerFeet; teamLastKnownEnemyAge_ = 0.0f; break; }
    }

    for (size_t bi = 0; bi < bots.size(); ++bi) {
        Bot& bot = bots[bi];
        if (!bot.alive || bot.state != BotState::Idle) continue;
        const BotDifficultyTuning tuning = tuningForDifficulty(bot.difficulty);

        bool heard = false;
        for (Vec3 s : soundsThisFrame) {
            float dx = s.x - bot.origin.x, dy = s.y - bot.origin.y, dz = s.z - bot.origin.z;
            if (std::sqrt(dx * dx + dy * dy + dz * dz) <= tuning.hearingRadius) {
                bot.state = BotState::Search;
                bot.investigateTarget = s;
                bot.investigateTimer = kSearchDuration;
                heard = true;
                break;
            }
        }
        if (!heard && teamLastKnownEnemyAge_ < kCalloutFreshness) {
            bot.state = BotState::Search;
            bot.investigateTarget = teamLastKnownEnemyPos_;
            bot.investigateTimer = kSearchDuration;
        }
    }

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
