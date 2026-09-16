#include "nav.h"

#include <algorithm>
#include <cmath>

namespace {

// Upper bound on total graph nodes — keeps the adjacency passes below (and
// the memory for their lists) bounded regardless of map size, the same
// "sanity cap on untrusted/unbounded input" convention assets/limits.h uses
// for file parsing.
constexpr int kMaxNodes = 160;
constexpr int kMinGridNodes = 30; // grid fallback always gets at least this much of the budget, even on a poly-rich map
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
constexpr float kFloorNormalZ = 0.7f;   // how upward-facing a face's normal must be to count as a floor
constexpr float kMinPolyArea = 400.0f;  // skip tiny/sliver faces (trim, detail brushes) as nav nodes
constexpr float kSharedEdgeEpsilon = 2.0f; // how close two faces' vertices must be to count as sharing an edge

constexpr int kBotNavHull = 1; // standing player hull

float dist2D(Vec3 a, Vec3 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

float dist3(Vec3 a, Vec3 b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vec3 crossProduct(Vec3 a, Vec3 b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// A face's outward normal from its first three vertices — BSP faces are
// convex, planar fans, so any three non-collinear vertices give the true
// plane normal. Returns a zero vector if the face is degenerate.
Vec3 faceNormal(const BspFace& face) {
    if (face.vertices.size() < 3) return Vec3{0, 0, 0};
    Vec3 v0 = face.vertices[0], v1 = face.vertices[1], v2 = face.vertices[2];
    Vec3 e1{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    Vec3 e2{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    Vec3 n = crossProduct(e1, e2);
    float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len < 1e-6f) return Vec3{0, 0, 0};
    return Vec3{n.x / len, n.y / len, n.z / len};
}

// Polygon area via a triangle-fan sum (face.vertices is already a convex
// fan, same order the renderer draws it in).
float faceArea(const BspFace& face) {
    float total = 0.0f;
    for (size_t i = 1; i + 1 < face.vertices.size(); ++i) {
        Vec3 a = face.vertices[0], b = face.vertices[i], c = face.vertices[i + 1];
        Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z}, ac{c.x - a.x, c.y - a.y, c.z - a.z};
        Vec3 cr = crossProduct(ab, ac);
        total += std::sqrt(cr.x * cr.x + cr.y * cr.y + cr.z * cr.z);
    }
    return total * 0.5f;
}

Vec3 faceCentroid(const BspFace& face) {
    Vec3 c{0, 0, 0};
    for (const Vec3& v : face.vertices) { c.x += v.x; c.y += v.y; c.z += v.z; }
    float inv = 1.0f / (float)face.vertices.size();
    return Vec3{c.x * inv, c.y * inv, c.z * inv};
}

// True if polygons a and b share an edge (two consecutive vertices in one
// matching two consecutive vertices in the other, in either winding order,
// within kSharedEdgeEpsilon) — real adjacency, the way GoldSrc brushes
// commonly split one floor into several touching faces along shared seams.
bool facesShareEdge(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    auto near = [](Vec3 p, Vec3 q) {
        float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
        return dx * dx + dy * dy + dz * dz <= kSharedEdgeEpsilon * kSharedEdgeEpsilon;
    };
    size_t na = a.size(), nb = b.size();
    for (size_t i = 0; i < na; ++i) {
        Vec3 a0 = a[i], a1 = a[(i + 1) % na];
        for (size_t j = 0; j < nb; ++j) {
            Vec3 b0 = b[j], b1 = b[(j + 1) % nb];
            if ((near(a0, b0) && near(a1, b1)) || (near(a0, b1) && near(a1, b0))) return true;
        }
    }
    return false;
}

} // namespace

void NavGraph::build(const BspMap& map, const std::vector<Vec3>& seeds, Vec3 mapMins, Vec3 mapMaxs) {
    nodes_.clear();
    adjacency_.clear();
    // Parallel to nodes_: the original world-space vertex ring for polygon
    // nodes (used only to test shared-edge adjacency below), empty for
    // seed/grid nodes.
    std::vector<std::vector<Vec3>> polyRings;

    for (Vec3 seed : seeds) {
        if ((int)nodes_.size() >= kMaxNodes) break;
        nodes_.push_back(NavNode{seed, false});
        polyRings.emplace_back();
    }

    // Real navmesh polygons: every floor-facing BSP face above a minimum
    // size, one node per face at its centroid, largest faces first so a
    // tight node budget still covers the map's main walkable areas.
    struct Candidate { Vec3 centroid; float area; const BspFace* face; };
    std::vector<Candidate> candidates;
    for (const BspFace& face : map.faces()) {
        if (face.vertices.size() < 3) continue;
        if (faceNormal(face).z < kFloorNormalZ) continue;
        float area = faceArea(face);
        if (area < kMinPolyArea) continue;
        candidates.push_back({faceCentroid(face), area, &face});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.area > b.area;
    });

    int budget = kMaxNodes - (int)nodes_.size();
    // Reserve part of the budget for the grid fallback below, so a
    // poly-rich map doesn't starve it entirely on maps where the floor
    // faces alone don't cover every walkable area (e.g. func_illusionary
    // floors, or areas this face filter is too conservative about).
    int polyLimit = std::max(0, budget - kMinGridNodes);
    polyLimit = std::min(polyLimit, (int)candidates.size());
    for (int i = 0; i < polyLimit; ++i) {
        Vec3 pos = candidates[i].centroid;
        pos.z += kStandoff;
        nodes_.push_back(NavNode{pos, true});
        polyRings.push_back(candidates[i].face->vertices);
    }

    // Grid fallback: floor-probed samples across the map bounds, same as
    // before polygon nodes existed — fills whatever budget is left, so a
    // map with few/no usable floor faces still gets *some* coverage.
    float spanX = mapMaxs.x - mapMins.x, spanY = mapMaxs.y - mapMins.y;
    if (spanX > 1.0f && spanY > 1.0f && (int)nodes_.size() < kMaxNodes) {
        int gridBudget = kMaxNodes - (int)nodes_.size();
        float area = spanX * spanY;
        float step = std::max(kMinGridStep, std::sqrt(area / (float)std::max(kMinGridNodes, gridBudget)));

        float probeTop = mapMaxs.z + kProbeMargin;
        float probeBottom = mapMins.z - kProbeMargin;

        for (float gx = mapMins.x; gx <= mapMaxs.x && (int)nodes_.size() < kMaxNodes; gx += step) {
            for (float gy = mapMins.y; gy <= mapMaxs.y && (int)nodes_.size() < kMaxNodes; gy += step) {
                Vec3 hit;
                Vec3 normal;
                bool didHit = map.traceLine(Vec3{gx, gy, probeTop}, Vec3{gx, gy, probeBottom}, hit, &normal);
                if (!didHit) continue;
                if (normal.z < kFloorNormalZ) continue;
                Vec3 candidate{hit.x, hit.y, hit.z + kStandoff};
                if (map.pointInSolidHull(candidate, kBotNavHull)) continue;
                nodes_.push_back(NavNode{candidate, false});
                polyRings.emplace_back();
            }
        }
    }

    adjacency_.assign(nodes_.size(), {});

    // Pass 1: real polygon-edge adjacency, regardless of distance — this is
    // the actual navmesh connectivity, not a guessed sightline.
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (polyRings[i].empty()) continue;
        for (size_t j = i + 1; j < nodes_.size(); ++j) {
            if (polyRings[j].empty()) continue;
            if (!facesShareEdge(polyRings[i], polyRings[j])) continue;
            adjacency_[i].push_back((int)j);
            adjacency_[j].push_back((int)i);
        }
    }

    // Pass 2: line-of-sight edges stitch in seeds/grid nodes (and anything
    // else within range that isn't already polygon-connected) — same
    // supplementary role this was the *only* connectivity source for
    // before polygon nodes existed.
    for (size_t i = 0; i < nodes_.size(); ++i) {
        for (size_t j = i + 1; j < nodes_.size(); ++j) {
            bool already = std::find(adjacency_[i].begin(), adjacency_[i].end(), (int)j) != adjacency_[i].end();
            if (already) continue;
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

    auto heuristic = [&](int n) { return dist3(nodes_[n].pos, nodes_[goal].pos); };

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
            float edgeCost = dist3(nodes_[current].pos, nodes_[neighbor].pos);
            float tentative = gScore[current] + edgeCost;
            if (tentative < gScore[neighbor]) {
                cameFrom[neighbor] = current;
                gScore[neighbor] = tentative;
                fScore[neighbor] = tentative + heuristic(neighbor);
            }
        }
    }

    if (cameFrom[goal] < 0) return {}; // never reached (start != goal, handled above)

    std::vector<Vec3> path;
    for (int n = goal; n != -1 && n != start; n = cameFrom[n]) {
        path.push_back(nodes_[n].pos);
    }
    std::reverse(path.begin(), path.end());
    return path;
}
