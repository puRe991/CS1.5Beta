#include "nav.h"

#include <algorithm>
#include <cmath>

namespace {

// Upper bound on total graph nodes — keeps the O(n^2) visibility-edge pass
// (and the memory for its adjacency lists) bounded regardless of map size,
// the same "sanity cap on untrusted/unbounded input" convention
// assets/limits.h uses for file parsing.
constexpr int kMaxNodes = 160;
constexpr int kMinGridNodes = 40; // below this, thin sampling wouldn't be worth much — widen the step less
constexpr float kMinGridStep = 128.0f;
constexpr float kProbeMargin = 64.0f;   // how far above/below the map bounds the floor probe starts/ends
// Lift a found floor point clear of the surface. traceLine samples in 4-unit
// steps, so the reported hit point can land up to ~4 units on the solid
// side of the actual boundary — this needs to be bigger than that margin or
// the "is there room to stand" recheck right below would immediately see
// solid again and reject every single candidate.
constexpr float kStandoff = 8.0f;
constexpr float kEyeCheckHeight = 40.0f; // height above floor a visibility check is done at (roughly chest height)
constexpr float kMaxEdgeDistance = 1400.0f; // don't even trace a visibility check beyond this — too far to matter

constexpr int kBotNavHull = 1; // standing player hull

float dist2D(Vec3 a, Vec3 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

void NavGraph::build(const BspMap& map, const std::vector<Vec3>& seeds, Vec3 mapMins, Vec3 mapMaxs) {
    nodes_.clear();
    adjacency_.clear();

    for (Vec3 seed : seeds) {
        if ((int)nodes_.size() >= kMaxNodes) break;
        nodes_.push_back(NavNode{seed});
    }

    float spanX = mapMaxs.x - mapMins.x, spanY = mapMaxs.y - mapMins.y;
    if (spanX > 1.0f && spanY > 1.0f && (int)nodes_.size() < kMaxNodes) {
        int budget = kMaxNodes - (int)nodes_.size();
        // Adapt grid spacing to the map's area so the sampled node count
        // stays within budget: step = sqrt(area / budget), floored at
        // kMinGridStep so a tiny map doesn't produce a silly-dense grid.
        float area = spanX * spanY;
        float step = std::max(kMinGridStep, std::sqrt(area / (float)std::max(kMinGridNodes, budget)));

        float probeTop = mapMaxs.z + kProbeMargin;
        float probeBottom = mapMins.z - kProbeMargin;

        for (float gx = mapMins.x; gx <= mapMaxs.x && (int)nodes_.size() < kMaxNodes; gx += step) {
            for (float gy = mapMins.y; gy <= mapMaxs.y && (int)nodes_.size() < kMaxNodes; gy += step) {
                Vec3 hit;
                Vec3 normal;
                bool didHit = map.traceLine(Vec3{gx, gy, probeTop}, Vec3{gx, gy, probeBottom}, hit, &normal);
                if (!didHit) continue;                 // no floor found straight down from here
                if (normal.z < 0.7f) continue;          // not a mostly-upward (floor-like) surface
                Vec3 candidate{hit.x, hit.y, hit.z + kStandoff};
                if (map.pointInSolidHull(candidate, kBotNavHull)) continue; // no room to stand
                nodes_.push_back(NavNode{candidate});
            }
        }
    }

    adjacency_.assign(nodes_.size(), {});
    for (size_t i = 0; i < nodes_.size(); ++i) {
        for (size_t j = i + 1; j < nodes_.size(); ++j) {
            if (dist2D(nodes_[i].pos, nodes_[j].pos) > kMaxEdgeDistance) continue;
            Vec3 a{nodes_[i].pos.x, nodes_[i].pos.y, nodes_[i].pos.z + kEyeCheckHeight};
            Vec3 b{nodes_[j].pos.x, nodes_[j].pos.y, nodes_[j].pos.z + kEyeCheckHeight};
            Vec3 hit;
            if (map.traceLine(a, b, hit, nullptr)) continue; // blocked
            adjacency_[i].push_back((int)j);
            adjacency_[j].push_back((int)i);
        }
    }
}

int NavGraph::nearestNode(Vec3 pos) const {
    int best = -1;
    float bestDist = 1e30f;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        float dx = nodes_[i].pos.x - pos.x, dy = nodes_[i].pos.y - pos.y, dz = nodes_[i].pos.z - pos.z;
        float d = dx * dx + dy * dy + dz * dz;
        if (d < bestDist) { bestDist = d; best = (int)i; }
    }
    return best;
}

std::vector<Vec3> NavGraph::findPath(Vec3 from, Vec3 to) const {
    if (nodes_.empty()) return {};
    int start = nearestNode(from);
    int goal = nearestNode(to);
    if (start < 0 || goal < 0) return {};
    if (start == goal) return {nodes_[goal].pos};

    // Small bounded graph (kMaxNodes nodes) — a linear scan for the lowest
    // f-score each step is simpler to get right than a heap and plenty fast
    // at this size; this isn't a per-frame hot path (paths are cached by
    // the caller and only recomputed occasionally).
    std::vector<float> gScore(nodes_.size(), 1e30f);
    std::vector<float> fScore(nodes_.size(), 1e30f);
    std::vector<int> cameFrom(nodes_.size(), -1);
    std::vector<bool> closed(nodes_.size(), false);

    auto heuristic = [&](int n) {
        Vec3 p = nodes_[n].pos, g = nodes_[goal].pos;
        float dx = p.x - g.x, dy = p.y - g.y, dz = p.z - g.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };

    gScore[start] = 0.0f;
    fScore[start] = heuristic(start);

    for (size_t iter = 0; iter < nodes_.size(); ++iter) {
        int current = -1;
        float bestF = 1e30f;
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (!closed[i] && fScore[i] < bestF) { bestF = fScore[i]; current = (int)i; }
        }
        if (current < 0) break; // nothing left reachable
        if (current == goal) break;
        closed[current] = true;

        for (int neighbor : adjacency_[current]) {
            if (closed[neighbor]) continue;
            Vec3 a = nodes_[current].pos, b = nodes_[neighbor].pos;
            float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            float edgeCost = std::sqrt(dx * dx + dy * dy + dz * dz);
            float tentative = gScore[current] + edgeCost;
            if (tentative < gScore[neighbor]) {
                cameFrom[neighbor] = current;
                gScore[neighbor] = tentative;
                fScore[neighbor] = tentative + heuristic(neighbor);
            }
        }
    }

    if (cameFrom[goal] < 0 && start != goal) return {}; // never reached

    std::vector<Vec3> path;
    for (int n = goal; n != -1 && n != start; n = cameFrom[n]) {
        path.push_back(nodes_[n].pos);
    }
    std::reverse(path.begin(), path.end());
    return path;
}
