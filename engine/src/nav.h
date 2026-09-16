#pragma once

#include <vector>

#include "assets/bsp.h"

// A real polygon navmesh's nodes, built from the map's own floor-facing BSP
// faces (kept as-is, not re-triangulated/merged) and connected by genuine
// shared-edge adjacency, not a guessed sightline — plus a supplementary
// layer for gameplay points that don't land exactly on a floor polygon
// (spawns, bomb targets, buy zones) and a floor-probed grid sample for maps
// where too few usable floor faces were found, both stitched in via
// line-of-sight edges. Still not hull-width-aware (a path can graze
// geometry a clearance-aware navmesh would route further from), has no
// ledge/jump links, and caps total node count for build-time safety — see
// kMaxNodes in nav.cpp — but the bulk of the graph on a normal map is real
// walkable geometry connected by real adjacency, not a heuristic.
struct NavNode {
    Vec3 pos; // a floor-level (feet) position, same convention as Bot::origin
    bool fromPolygon = false; // true if this node came from a real BSP floor face, not a seed/grid sample
};

class NavGraph {
public:
    // Builds the graph: `seeds` (always included as nodes, since they're
    // known-walkable gameplay points) plus a grid sample across
    // [mapMins, mapMaxs] at floor level, found by probing straight down
    // from above the map and keeping points that land on a mostly-upward
    // surface with room to stand. Grid spacing adapts to the map's area so
    // total node count (and the O(n^2) visibility check that follows) stays
    // bounded — see kMaxNodes in nav.cpp.
    void build(const BspMap& map, const std::vector<Vec3>& seeds, Vec3 mapMins, Vec3 mapMaxs);

    bool empty() const { return nodes_.empty(); }
    const std::vector<NavNode>& nodes() const { return nodes_; }

    // Index of the node nearest `pos` (by straight-line distance), or -1 if
    // the graph has no nodes at all.
    int nearestNode(Vec3 pos) const;

    // A* shortest path by edge distance from the node nearest `from` to the
    // node nearest `to`. Returns waypoint positions in travel order,
    // *excluding* the start but *including* the goal node's position —
    // empty if the graph is empty or no path exists (disconnected areas).
    std::vector<Vec3> findPath(Vec3 from, Vec3 to) const;

private:
    std::vector<NavNode> nodes_;
    std::vector<std::vector<int>> adjacency_; // neighbor node indices, parallel to nodes_
};
