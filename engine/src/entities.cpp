#include "entities.h"

#include <cstdio>
#include <cstdlib>

namespace {

float parseYaw(const BspEntity& ent) {
    // Most entities use "angles" as "pitch yaw roll"; some older ones use a
    // single "angle" field holding just the yaw. Handle both.
    if (const std::string* angles = ent.get("angles")) {
        float pitch = 0, yaw = 0, roll = 0;
        std::sscanf(angles->c_str(), "%f %f %f", &pitch, &yaw, &roll);
        return yaw;
    }
    if (const std::string* angle = ent.get("angle")) {
        return (float)std::atof(angle->c_str());
    }
    return 0.0f;
}

Vec3 parseOrigin(const BspEntity& ent) {
    Vec3 v{0, 0, 0};
    if (const std::string* origin = ent.get("origin")) {
        std::sscanf(origin->c_str(), "%f %f %f", &v.x, &v.y, &v.z);
    }
    return v;
}

Team parseTeamKey(const BspEntity& ent) {
    const std::string* team = ent.get("team");
    if (!team) return Team::Unknown;
    // GoldSrc/CS convention: team 1 = Terrorist, team 2 = Counter-Terrorist.
    if (*team == "1") return Team::T;
    if (*team == "2") return Team::CT;
    return Team::Unknown;
}

} // namespace

bool pointInZone(const ZoneRegion& zone, Vec3 p) {
    return p.x >= zone.mins.x && p.x <= zone.maxs.x &&
           p.y >= zone.mins.y && p.y <= zone.maxs.y &&
           p.z >= zone.mins.z && p.z <= zone.maxs.z;
}

void EntitySystem::build(const BspMap& map) {
    spawns.clear();
    bombTargets.clear();
    buyZones.clear();

    for (const auto& ent : map.entities()) {
        const std::string* classname = ent.get("classname");
        if (!classname) continue;

        if (*classname == "info_player_start" || *classname == "info_player_deathmatch") {
            SpawnPoint sp;
            sp.origin = parseOrigin(ent);
            sp.yaw = parseYaw(ent);
            // Historical GoldSrc-CS convention (predates dedicated T/CT spawn
            // classnames): info_player_start = CT, info_player_deathmatch = T.
            sp.team = (*classname == "info_player_start") ? Team::CT : Team::T;
            spawns.push_back(sp);

        } else if (*classname == "func_bomb_target" || *classname == "func_buyzone") {
            int modelIdx = BspMap::modelIndexFor(ent);
            if (modelIdx < 0 || (size_t)modelIdx >= map.models().size()) continue;

            ZoneRegion zone;
            zone.mins = map.models()[modelIdx].mins;
            zone.maxs = map.models()[modelIdx].maxs;
            zone.team = parseTeamKey(ent);

            if (*classname == "func_bomb_target") bombTargets.push_back(zone);
            else buyZones.push_back(zone);
        }
    }
}
