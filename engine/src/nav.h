#pragma once

#include <vector>

#include "assets/bsp.h"

// A coarse stand-in for a real navmesh: a visibility graph over a handful
// of "seed" points (spawns, bomb targets, buy zones — supplied by the
// caller) plus a floor-probed grid sample across the map's bounds, with an
// edge between any two nodes that have an unobstructed line of sight
// between them. Not polygonal, not aware of hull width beyond "can a bullet
// see between these two floor points" (so a path can still graze geometry
// a real navmesh would route further from) — but real routing around walls
// instead of the straight-line steering bots used to do, and enough to
// unstick a bot from a corner a direct line would drive it into.
struct NavNode {
    Vec3 pos; // a floor-level (feet) position, same convention as Bot::origin
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
