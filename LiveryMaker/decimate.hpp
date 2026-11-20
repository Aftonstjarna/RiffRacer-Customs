#pragma once
// Quadric edge-collapse simplification with UV-seam and boundary protection.
// Operates on a standalone triangle soup so it can be tested without the game.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <vector>

namespace livery {
namespace decimate {

using Vec = std::array<double, 3>;
using Uv = std::array<double, 2>;
struct Mesh {
    std::vector<Vec> positions;
    std::vector<Vec> normals;
    std::vector<Uv> uv;
    std::vector<uint32_t> indices;
};
using Quadric = std::array<double, 10>;

inline void addPlane(Quadric &q, double a, double b, double c, double d, double weight) {
    q[0] += weight * a * a; q[1] += weight * a * b; q[2] += weight * a * c; q[3] += weight * a * d;
    q[4] += weight * b * b; q[5] += weight * b * c; q[6] += weight * b * d;
    q[7] += weight * c * c; q[8] += weight * c * d; q[9] += weight * d * d;
}
inline double error(const Quadric &q, const Vec &p) {
    return q[0] * p[0] * p[0] + 2 * q[1] * p[0] * p[1] + 2 * q[2] * p[0] * p[2] + 2 * q[3] * p[0] +
           q[4] * p[1] * p[1] + 2 * q[5] * p[1] * p[2] + 2 * q[6] * p[1] +
           q[7] * p[2] * p[2] + 2 * q[8] * p[2] + q[9];
}
inline Vec subtract(const Vec &a, const Vec &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
inline Vec cross(const Vec &a, const Vec &b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
inline double dot(const Vec &a, const Vec &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline Vec unit(const Vec &v) {
    double length = std::sqrt(dot(v, v));
    return length > 1e-15 ? Vec{v[0] / length, v[1] / length, v[2] / length} : Vec{0, 0, 1};
}
inline double uvArea(const Uv &a, const Uv &b, const Uv &c) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
}

struct Vertex {
    Vec p, n;
    Uv uv{};
    Quadric q{};
    bool alive = true;
    uint32_t version = 0;
    std::vector<uint32_t> triangles;
};
struct Triangle {
    uint32_t v[3]{};
    bool alive = true;
};

struct Edge {
    double priority;
    uint32_t a, b, va, vb;
};
struct Later {
    bool operator()(const Edge &x, const Edge &y) const { return x.priority > y.priority; }
};

inline void simplify(Mesh &mesh, size_t targetTriangles) {
    size_t triangleCount = mesh.indices.size() / 3;
    if (targetTriangles >= triangleCount || mesh.positions.size() > 200000) return;
    targetTriangles = std::max<size_t>(4, targetTriangles);
    std::vector<Vertex> vertices(mesh.positions.size());
    for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i].p = mesh.positions[i];
        vertices[i].n = i < mesh.normals.size() ? mesh.normals[i] : Vec{0, 0, 0};
        vertices[i].uv = i < mesh.uv.size() ? mesh.uv[i] : Uv{0, 0};
    }
    std::vector<Triangle> triangles(triangleCount);
    for (size_t i = 0; i < triangleCount; i++) {
        triangles[i].v[0] = mesh.indices[i * 3];
        triangles[i].v[1] = mesh.indices[i * 3 + 1];
        triangles[i].v[2] = mesh.indices[i * 3 + 2];
        for (uint32_t v : triangles[i].v) {
            if (v >= vertices.size()) return;
            vertices[v].triangles.push_back(uint32_t(i));
        }
    }
    const double areaScale = 1.0;
    for (size_t i = 0; i < triangles.size(); i++) {
        const auto &t = triangles[i];
        const Vec &a = vertices[t.v[0]].p, &b = vertices[t.v[1]].p, &c = vertices[t.v[2]].p;
        Vec normal = cross(subtract(b, a), subtract(c, a));
        double area = std::sqrt(dot(normal, normal));
        if (area < 1e-18) continue;
        normal = {normal[0] / area, normal[1] / area, normal[2] / area};
        double d = -dot(normal, a);
        for (uint32_t v : t.v) addPlane(vertices[v].q, normal[0], normal[1], normal[2], d, area * areaScale);
    }
    // Boundary and UV-seam edges get a perpendicular constraint plane.
    std::map<std::pair<uint32_t, uint32_t>, std::pair<int, uint32_t>> edges;
    for (size_t i = 0; i < triangles.size(); i++) {
        const auto &t = triangles[i];
        for (int k = 0; k < 3; k++) {
            uint32_t a = std::min(t.v[k], t.v[(k + 1) % 3]), b = std::max(t.v[k], t.v[(k + 1) % 3]);
            auto &entry = edges[{a, b}];
            entry.first++;
            entry.second = uint32_t(i);
        }
    }
    for (const auto &entry : edges) {
        if (entry.second.first != 1) continue;
        const auto &t = triangles[entry.second.second];
        const Vec &a = vertices[entry.first.first].p, &b = vertices[entry.first.second].p;
        Vec edge = subtract(b, a);
        Vec face = cross(subtract(vertices[t.v[1]].p, vertices[t.v[0]].p),
                         subtract(vertices[t.v[2]].p, vertices[t.v[0]].p));
        Vec plane = unit(cross(face, edge));
        double weight = 1000.0 * dot(edge, edge);
        double d = -dot(plane, a);
        for (uint32_t v : {entry.first.first, entry.first.second})
            addPlane(vertices[v].q, plane[0], plane[1], plane[2], d, weight);
    }
    auto neighbors = [&](uint32_t v) {
        std::set<uint32_t> result;
        for (uint32_t ti : vertices[v].triangles)
            if (triangles[ti].alive)
                for (uint32_t w : triangles[ti].v)
                    if (w != v) result.insert(w);
        return result;
    };
    std::priority_queue<Edge, std::vector<Edge>, Later> queue;
    auto priority = [&](uint32_t a, uint32_t b) {
        Quadric q;
        for (int i = 0; i < 10; i++) q[i] = vertices[a].q[i] + vertices[b].q[i];
        return std::min(error(q, vertices[a].p), error(q, vertices[b].p));
    };
    auto push = [&](uint32_t a, uint32_t b) {
        if (a == b || !vertices[a].alive || !vertices[b].alive) return;
        if (a > b) std::swap(a, b);
        queue.push({priority(a, b), a, b, vertices[a].version, vertices[b].version});
    };
    for (const auto &entry : edges) push(entry.first.first, entry.first.second);
    size_t alive = triangles.size();
    while (!queue.empty() && alive > targetTriangles) {
        Edge edge = queue.top();
        queue.pop();
        if (!vertices[edge.a].alive || !vertices[edge.b].alive) continue;
        if (vertices[edge.a].version != edge.va || vertices[edge.b].version != edge.vb) continue;
        Quadric combined;
        for (int i = 0; i < 10; i++) combined[i] = vertices[edge.a].q[i] + vertices[edge.b].q[i];
        bool keepA = error(combined, vertices[edge.a].p) <= error(combined, vertices[edge.b].p);
        uint32_t keep = keepA ? edge.a : edge.b, remove = keepA ? edge.b : edge.a;
        auto ka = neighbors(keep), rb = neighbors(remove);
        std::set<uint32_t> shared;
        for (uint32_t v : rb) if (ka.count(v)) shared.insert(v);
        size_t edgeTriangles = 0;
        for (uint32_t ti : vertices[remove].triangles)
            if (triangles[ti].alive && std::find(triangles[ti].v, triangles[ti].v + 3, keep) != triangles[ti].v + 3) edgeTriangles++;
        if (shared.size() != edgeTriangles) continue;
        bool valid = true;
        for (uint32_t ti : vertices[remove].triangles) {
            if (!triangles[ti].alive) continue;
            auto &t = triangles[ti];
            if (t.v[0] == keep || t.v[1] == keep || t.v[2] == keep) continue;
            uint32_t old[3] = {t.v[0], t.v[1], t.v[2]}, next[3];
            for (int k = 0; k < 3; k++) next[k] = old[k] == remove ? keep : old[k];
            Vec oldNormal = cross(subtract(vertices[old[1]].p, vertices[old[0]].p),
                                  subtract(vertices[old[2]].p, vertices[old[0]].p));
            Vec newNormal = cross(subtract(vertices[next[1]].p, vertices[next[0]].p),
                                  subtract(vertices[next[2]].p, vertices[next[0]].p));
            double oldLength = std::sqrt(dot(oldNormal, oldNormal)), newLength = std::sqrt(dot(newNormal, newNormal));
            if (oldLength < 1e-18 || newLength < 1e-18 || dot(oldNormal, newNormal) <= 0.05 * oldLength * newLength) {valid = false; break;}
            double oldArea = uvArea(vertices[old[0]].uv, vertices[old[1]].uv, vertices[old[2]].uv);
            double newArea = uvArea(vertices[next[0]].uv, vertices[next[1]].uv, vertices[next[2]].uv);
            if (std::abs(oldArea) > 1e-12 && (newArea * oldArea <= 0 || std::abs(newArea) < std::abs(oldArea) * 0.05)) {valid = false; break;}
        }
        if (!valid) continue;
        for (int i = 0; i < 10; i++) vertices[keep].q[i] += vertices[remove].q[i];
        for (uint32_t ti : vertices[remove].triangles) {
            if (!triangles[ti].alive) continue;
            auto &t = triangles[ti];
            if (t.v[0] == keep || t.v[1] == keep || t.v[2] == keep) {t.alive = false; alive--; continue;}
            for (int k = 0; k < 3; k++) if (t.v[k] == remove) t.v[k] = keep;
            vertices[keep].triangles.push_back(ti);
        }
        vertices[remove].alive = false;
        vertices[remove].triangles.clear();
        vertices[keep].version++;
        std::set<uint32_t> around = neighbors(keep);
        for (uint32_t v : around) push(keep, v);
    }
    // Compact survivors.
    std::vector<uint32_t> remap(vertices.size(), UINT32_MAX);
    Mesh result;
    for (size_t i = 0; i < vertices.size(); i++) {
        if (!vertices[i].alive) continue;
        remap[i] = uint32_t(result.positions.size());
        result.positions.push_back(vertices[i].p);
        result.normals.push_back(vertices[i].n);
        result.uv.push_back(vertices[i].uv);
    }
    for (const auto &t : triangles) {
        if (!t.alive) continue;
        uint32_t a = remap[t.v[0]], b = remap[t.v[1]], c = remap[t.v[2]];
        if (a == UINT32_MAX || b == UINT32_MAX || c == UINT32_MAX || a == b || b == c || a == c) continue;
        result.indices.insert(result.indices.end(), {a, b, c});
    }
    if (result.indices.size() >= 3) mesh = std::move(result);
}

} // namespace decimate
} // namespace livery
