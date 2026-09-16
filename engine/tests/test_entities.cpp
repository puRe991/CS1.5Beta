#include "test_framework.h"
#include "fixtures.h"

#include "entities.h"

TEST(point_in_zone_bounds_are_inclusive) {
    ZoneRegion zone;
    zone.mins = {0, 0, 0};
    zone.maxs = {10, 10, 10};

    CHECK(pointInZone(zone, Vec3{5, 5, 5}));
    CHECK(pointInZone(zone, Vec3{0, 0, 0}));
    CHECK(pointInZone(zone, Vec3{10, 10, 10}));
    CHECK(!pointInZone(zone, Vec3{11, 5, 5}));
    CHECK(!pointInZone(zone, Vec3{5, -1, 5}));
}

// Builds a tiny map with one CT spawn, one T spawn, and a bomb-target brush
// entity (*0) whose model bounds come from the one BspModelBounds we define,
// then checks EntitySystem::build classifies all of it correctly.
TEST(entity_system_build_classifies_spawns_and_zones) {
    fixtures::BspBuilder builder;
    builder.entityText =
        "{\n\"classname\" \"worldspawn\"\n}\n"
        "{\n\"classname\" \"info_player_start\"\n\"origin\" \"1 2 3\"\n\"angle\" \"90\"\n}\n"
        "{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"4 5 6\"\n\"angles\" \"0 180 0\"\n}\n"
        "{\n\"classname\" \"func_bomb_target\"\n\"model\" \"*0\"\n}\n"
        "{\n\"classname\" \"func_buyzone\"\n\"model\" \"*0\"\n\"team\" \"2\"\n}\n";

    fixtures::BspDModel model{};
    model.mins[0] = -1; model.mins[1] = -2; model.mins[2] = -3;
    model.maxs[0] = 1; model.maxs[1] = 2; model.maxs[2] = 3;
    builder.models.push_back(model);

    std::string path = builder.build();
    BspMap map;
    CHECK(map.load(path, {}));

    EntitySystem sys;
    sys.build(map);

    CHECK_EQ(sys.spawns.size(), (size_t)2);
    CHECK_EQ(sys.bombTargets.size(), (size_t)1);
    CHECK_EQ(sys.buyZones.size(), (size_t)1);

    // info_player_start -> CT, using "angle" (single-value yaw).
    CHECK(sys.spawns[0].team == Team::CT);
    CHECK_NEAR(sys.spawns[0].yaw, 90.0, 1e-4);
    CHECK_NEAR(sys.spawns[0].origin.x, 1.0, 1e-4);

    // info_player_deathmatch -> T, using "angles" (pitch yaw roll).
    CHECK(sys.spawns[1].team == Team::T);
    CHECK_NEAR(sys.spawns[1].yaw, 180.0, 1e-4);

    CHECK_NEAR(sys.bombTargets[0].mins.x, -1.0, 1e-4);
    CHECK_NEAR(sys.bombTargets[0].maxs.z, 3.0, 1e-4);
    CHECK(sys.bombTargets[0].team == Team::Unknown); // no "team" key -> applies to both

    CHECK(sys.buyZones[0].team == Team::CT); // team "2" -> CT
}

TEST(entity_system_build_ignores_entities_without_classname) {
    fixtures::BspBuilder builder;
    builder.entityText = "{\n\"origin\" \"1 2 3\"\n}\n";
    std::string path = builder.build();

    BspMap map;
    CHECK(map.load(path, {}));

    EntitySystem sys;
    sys.build(map);
    CHECK_EQ(sys.spawns.size(), (size_t)0);
}

int main() { return RUN_ALL_TESTS(); }
