// Quadric decimation: triangle budget, bounds, indices and small-mesh handling.
#include "decimate.hpp"
#include <iostream>

using namespace livery::decimate;

#define CHECK(condition) do {if (!(condition)) {std::cerr << "FAIL " << __LINE__ << ": " #condition "\n"; return 1;}} while (0)

int main() {
    Mesh mesh;
    const int n = 20;
    for (int y = 0; y <= n; y++)
        for (int x = 0; x <= n; x++) {
            mesh.positions.push_back({double(x) / n, double(y) / n, 0});
            mesh.normals.push_back({0, 0, 1});
            mesh.uv.push_back({double(x) / n, double(y) / n});
        }
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            uint32_t a = y * (n + 1) + x, b = a + 1, c = a + n + 1, d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, d, a, d, c});
        }
    size_t before = mesh.indices.size() / 3;
    simplify(mesh, 200);
    size_t after = mesh.indices.size() / 3;
    CHECK(after <= 200 && after >= 50);
    for (uint32_t index : mesh.indices) CHECK(index < mesh.positions.size());
    for (const auto &position : mesh.positions) {
        CHECK(position[0] >= -1e-9 && position[0] <= 1 + 1e-9);
        CHECK(position[1] >= -1e-9 && position[1] <= 1 + 1e-9);
        CHECK(std::abs(position[2]) < 1e-9);
    }
    Mesh small;
    small.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    small.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    small.uv = {{0, 0}, {1, 0}, {0, 1}};
    small.indices = {0, 1, 2};
    simplify(small, 100);
    CHECK(small.indices.size() == 3);
    std::cout << "PASS quadric decimation " << before << " -> " << after << " triangles\n";
    return 0;
}
