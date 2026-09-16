#include "test_framework.h"
#include "fixtures.h"

#include "bots.h"

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

int main() { return RUN_ALL_TESTS(); }
