#include "test_framework.h"
#include "fixtures.h"

#include "bots.h"
#include "weapons.h"

namespace {
// A wide-open map: no clipnodes/models at all, so pointInSolidHull() and
// traceLine() both report "nothing solid, nothing in the way" everywhere —
// exactly the free-movement, clear-line-of-sight scenario these tests want,
// without needing real BSP geometry.
std::string buildOpenMapWithSpawns() {
    fixtures::BspBuilder builder;
    builder.entityText =
        "{\n\"classname\" \"worldspawn\"\n}\n"
        "{\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 0\"\n\"angle\" \"0\"\n}\n"
        "{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"500 0 0\"\n\"angle\" \"180\"\n}\n";
    return builder.build();
}
} // namespace

TEST(bot_spawn_places_bots_on_their_teams_spawn) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);

    BotSystem botSystem;
    botSystem.spawn(3, Team::T, entities);
    CHECK_EQ(botSystem.bots.size(), (size_t)3);
    CHECK_EQ(botSystem.aliveCount(), 3);
    for (const Bot& b : botSystem.bots) {
        CHECK(b.alive);
        CHECK(b.team == Team::T);
        CHECK_NEAR(b.origin.x, 500.0, 1e-3); // the T spawn point
    }
}

TEST(bot_damage_kills_and_respawn_restores) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);

    BotSystem botSystem;
    botSystem.spawn(2, Team::T, entities);

    CHECK(!botSystem.damage(0, 10)); // 100 -> 90, not dead yet
    CHECK_EQ(botSystem.bots[0].health, 90);
    CHECK(botSystem.damage(0, 1000)); // massively overkill, still just dies once
    CHECK(!botSystem.bots[0].alive);
    CHECK_EQ(botSystem.bots[0].health, 0);
    CHECK_EQ(botSystem.aliveCount(), 1);

    CHECK(!botSystem.damage(0, 10)); // already dead: no-op, not a double kill
    CHECK(!botSystem.damage(99, 10)); // out of range: no-op, not a crash

    botSystem.respawnAll(entities);
    CHECK_EQ(botSystem.aliveCount(), 2);
    CHECK(botSystem.bots[0].alive);
    CHECK_EQ(botSystem.bots[0].health, kMaxHealth);
}

TEST(transform_hitboxes_to_world_rotates_then_translates) {
    WorldHitbox local;
    local.part = BodyPart::Chest;
    local.mins[0] = 0; local.mins[1] = 0; local.mins[2] = 0;
    local.maxs[0] = 10; local.maxs[1] = 2; local.maxs[2] = 2;

    std::vector<WorldHitbox> out = transformHitboxesToWorld({local}, Vec3{100, 50, 0}, 90.0f);
    CHECK_EQ(out.size(), (size_t)1);
    // yaw=90: world x = -local y + originX, world y = local x + originY.
    CHECK_NEAR(out[0].mins[0], 100.0f - 2.0f, 1e-3f);
    CHECK_NEAR(out[0].maxs[0], 100.0f, 1e-3f);
    CHECK_NEAR(out[0].mins[1], 50.0f, 1e-3f);
    CHECK_NEAR(out[0].maxs[1], 50.0f + 10.0f, 1e-3f);
    CHECK_NEAR(out[0].mins[2], 0.0f, 1e-3f);
    CHECK_NEAR(out[0].maxs[2], 2.0f, 1e-3f);
    CHECK(out[0].part == BodyPart::Chest);
}

TEST(bot_engages_and_fires_at_a_nearby_visible_player) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);
    BrushEntitySystem brushEntities;
    brushEntities.build(map);

    BotSystem botSystem;
    botSystem.spawn(1, Team::T, entities); // spawns at (500, 0, 0)

    PlayerState player;
    Vec3 playerFeet{500, 100, 0}; // 100 units away, well within engage range
    Vec3 playerEye{500, 100, 40};

    bool fired = false;
    for (int i = 0; i < 30 && !fired; ++i) {
        std::vector<BotFiredEvent> events = botSystem.update(1.0f / 30.0f, map, brushEntities, entities, player, playerEye, playerFeet);
        if (!events.empty()) fired = true;
    }
    CHECK(fired);
    CHECK(botSystem.bots[0].state == BotState::Attack);
}

TEST(bot_does_not_engage_a_player_far_out_of_view_distance) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);
    BrushEntitySystem brushEntities;
    brushEntities.build(map);

    BotSystem botSystem;
    botSystem.spawn(1, Team::T, entities);

    PlayerState player;
    Vec3 playerFeet{500, 100000, 0}; // far beyond any reasonable view distance
    Vec3 playerEye{500, 100000, 40};
    int startHealth = player.health;

    bool fired = false;
    for (int i = 0; i < 30; ++i) {
        std::vector<BotFiredEvent> events = botSystem.update(1.0f / 30.0f, map, brushEntities, entities, player, playerEye, playerFeet);
        if (!events.empty()) fired = true;
    }
    CHECK(!fired);
    CHECK(botSystem.bots[0].state == BotState::Idle);
    CHECK_EQ(player.health, startHealth);
}

TEST(bot_spawn_buys_the_priciest_affordable_pistol_when_no_primary_fits) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);

    BotSystem botSystem;
    botSystem.spawn(1, Team::T, entities); // starting money (800) can't afford any primary weapon

    const Bot& b = botSystem.bots[0];
    const WeaponDef& bought = weaponByIndex(b.weaponIndex);
    CHECK(bought.category == WeaponCategory::Pistol);
    CHECK_EQ(b.money, kStartingMoney - bought.price);

    // It really is the best affordable pistol, not just *a* pistol.
    int expectedPrice = -1;
    for (int i = 0; i < kWeaponCatalogCount; ++i) {
        const WeaponDef& w = kWeaponCatalog[i];
        if (w.category == WeaponCategory::Pistol && w.price <= kStartingMoney) {
            expectedPrice = std::max(expectedPrice, w.price);
        }
    }
    CHECK_EQ(bought.price, expectedPrice);
}

TEST(bot_respawn_credits_round_reward_before_rebuying) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);

    BotSystem botSystem;
    botSystem.spawn(1, Team::T, entities);
    int moneyAfterFirstBuy = botSystem.bots[0].money;

    botSystem.respawnAll(entities);
    // Credited kRoundMoneyReward, then spent some of it again on a re-buy —
    // so it should never have *less* than before, even after buying again.
    CHECK(botSystem.bots[0].money >= moneyAfterFirstBuy);
    CHECK(botSystem.bots[0].alive);
}

TEST(bot_reaction_time_delays_the_first_shot) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);
    BrushEntitySystem brushEntities;
    brushEntities.build(map);

    BotSystem botSystem;
    botSystem.spawn(1, Team::T, entities, BotDifficulty::Beginner); // ~1.2s reaction time

    PlayerState player;
    Vec3 playerFeet{500, 100, 0};
    Vec3 playerEye{500, 100, 40};

    bool firedEarly = false;
    float elapsed = 0.0f;
    const float dt = 1.0f / 30.0f;
    for (; elapsed < 1.0f; elapsed += dt) { // well under the ~1.2s reaction delay
        std::vector<BotFiredEvent> events = botSystem.update(dt, map, brushEntities, entities, player, playerEye, playerFeet);
        if (!events.empty()) firedEarly = true;
    }
    CHECK(!firedEarly);
    CHECK(botSystem.bots[0].state == BotState::Attack); // aware and moving, just not shooting yet

    bool firedLater = false;
    for (int i = 0; i < 30 && !firedLater; ++i) {
        std::vector<BotFiredEvent> events = botSystem.update(dt, map, brushEntities, entities, player, playerEye, playerFeet);
        if (!events.empty()) firedLater = true;
    }
    CHECK(firedLater);
}

TEST(bot_hears_a_reported_sound_and_investigates_without_los) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);
    BrushEntitySystem brushEntities;
    brushEntities.build(map);

    BotSystem botSystem;
    botSystem.spawn(1, Team::T, entities, BotDifficulty::Normal); // spawns at (500, 0, 0)

    PlayerState player;
    Vec3 playerFeet{500, 1000000, 0}; // far out of view/engage range
    Vec3 playerEye{500, 1000000, 40};
    std::vector<Vec3> sounds = {Vec3{500, 200, 0}}; // well within Normal's hearing radius of the bot

    botSystem.update(1.0f / 30.0f, map, brushEntities, entities, player, playerEye, playerFeet, sounds);
    CHECK(botSystem.bots[0].state == BotState::Search);
    CHECK_NEAR(botSystem.bots[0].investigateTarget.x, 500.0, 1e-3);
    CHECK_NEAR(botSystem.bots[0].investigateTarget.y, 200.0, 1e-3);
}

TEST(bot_team_callout_sends_idle_teammates_toward_a_teammates_sighting) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);
    BrushEntitySystem brushEntities;
    brushEntities.build(map);

    BotSystem botSystem;
    botSystem.spawn(1, Team::T, entities);
    // A second bot, far from both the player and the first bot — too far to
    // see or hear anything itself, so any reaction it has must have come
    // from the team callout, not its own perception.
    Bot farBot;
    farBot.team = Team::T;
    farBot.origin = Vec3{500, -50000, 0};
    botSystem.bots.push_back(farBot);

    PlayerState player;
    Vec3 playerFeet{500, 100, 0}; // close to bot 0 only
    Vec3 playerEye{500, 100, 40};

    for (int i = 0; i < 30; ++i) {
        botSystem.update(1.0f / 30.0f, map, brushEntities, entities, player, playerEye, playerFeet);
    }
    CHECK(botSystem.bots[0].state == BotState::Attack);
    CHECK(botSystem.bots[1].state == BotState::Search);
}

TEST(bot_populates_a_path_from_the_nav_graph_when_investigating) {
    std::string path = buildOpenMapWithSpawns();
    BspMap map;
    CHECK(map.load(path, {}));
    EntitySystem entities;
    entities.build(map);
    BrushEntitySystem brushEntities;
    brushEntities.build(map);

    BotSystem botSystem;
    botSystem.spawn(1, Team::T, entities); // spawns at (500, 0, 0)
    // Seeds the graph from the map's own spawn points (the only entities
    // this fixture has) — multi-hop routing itself is exercised thoroughly
    // in test_nav.cpp; this just checks BotSystem actually wires a bot's
    // Search movement through whatever graph buildNav() produced, instead
    // of silently ignoring it.
    botSystem.buildNav(map, entities, Vec3{-10, -10, -10}, Vec3{510, 10, 10});

    PlayerState player;
    Vec3 playerFeet{500, 1000000, 0};
    Vec3 playerEye{500, 1000000, 40};
    std::vector<Vec3> sounds = {Vec3{500, 200, 0}};

    botSystem.update(1.0f / 30.0f, map, brushEntities, entities, player, playerEye, playerFeet, sounds);
    CHECK(botSystem.bots[0].state == BotState::Search);
    CHECK(!botSystem.bots[0].path.empty());
}

int main() { return RUN_ALL_TESTS(); }
