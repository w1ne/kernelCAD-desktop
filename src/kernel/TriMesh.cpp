#include "TriMesh.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <unordered_map>
#include <utility>

namespace kernel::trimesh {

namespace {

struct Vec3 {
    double x, y, z;
};

Vec3 positionOf(const std::vector<float>& v, uint32_t i)
{
    return { v[3 * i], v[3 * i + 1], v[3 * i + 2] };
}

Vec3 sub(const Vec3& a, const Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

double dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double length(const Vec3& a)
{
    return std::sqrt(dot(a, a));
}

// Integer index of a grid cube.
struct Cell {
    int64_t x, y, z;
    bool operator==(const Cell& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct CellHash {
    size_t operator()(const Cell& c) const
    {
        size_t h = std::hash<int64_t>{}(c.x);
        h = h * 1000003u ^ std::hash<int64_t>{}(c.y);
        h = h * 1000003u ^ std::hash<int64_t>{}(c.z);
        return h;
    }
};

bool indicesInRange(const std::vector<uint32_t>& indices, size_t vertexCount)
{
    return std::all_of(indices.begin(), indices.end(),
                       [vertexCount](uint32_t i) { return i < vertexCount; });
}

} // namespace

void weld(std::vector<float>& vertices, std::vector<uint32_t>& indices, double tolerance)
{
    const size_t count = vertices.size() / 3;
    if (count == 0 || !(tolerance > 0.0))
        return;

    // Vertices are binned into cubes `tolerance` wide. Two vertices closer than
    // `tolerance` therefore sit in the same or adjacent cubes, so a lookup only
    // needs the 27 cubes around the vertex.
    constexpr uint32_t kNone = std::numeric_limits<uint32_t>::max();
    const double tolerance2 = tolerance * tolerance;
    std::vector<float> unique;  // welded positions, in first-seen order
    unique.reserve(vertices.size());
    std::unordered_map<Cell, std::vector<uint32_t>, CellHash> grid;
    std::vector<uint32_t> remap(count);

    auto findWithin = [&](const Vec3& p, const Cell& c) {
        for (int64_t dx = -1; dx <= 1; ++dx)
            for (int64_t dy = -1; dy <= 1; ++dy)
                for (int64_t dz = -1; dz <= 1; ++dz) {
                    auto it = grid.find(Cell{ c.x + dx, c.y + dy, c.z + dz });
                    if (it == grid.end()) continue;
                    for (uint32_t r : it->second) {
                        const Vec3 d = sub(p, positionOf(unique, r));
                        if (dot(d, d) <= tolerance2) return r;
                    }
                }
        return kNone;
    };

    for (size_t i = 0; i < count; ++i) {
        const Vec3 p = positionOf(vertices, static_cast<uint32_t>(i));
        const Cell cell{ static_cast<int64_t>(std::floor(p.x / tolerance)),
                         static_cast<int64_t>(std::floor(p.y / tolerance)),
                         static_cast<int64_t>(std::floor(p.z / tolerance)) };

        uint32_t found = findWithin(p, cell);
        if (found == kNone) {
            found = static_cast<uint32_t>(unique.size() / 3);
            unique.push_back(vertices[3 * i]);
            unique.push_back(vertices[3 * i + 1]);
            unique.push_back(vertices[3 * i + 2]);
            grid[cell].push_back(found);
        }
        remap[i] = found;
    }

    std::vector<uint32_t> kept;
    kept.reserve(indices.size());
    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        const uint32_t a = indices[t], b = indices[t + 1], c = indices[t + 2];
        if (a >= count || b >= count || c >= count)
            continue;  // refers to a vertex that does not exist
        const uint32_t ra = remap[a], rb = remap[b], rc = remap[c];
        if (ra == rb || rb == rc || ra == rc)
            continue;  // collapsed onto an edge or a point
        kept.insert(kept.end(), { ra, rb, rc });
    }

    vertices = std::move(unique);
    indices = std::move(kept);
}

MeshReport analyze(const std::vector<float>& vertices, const std::vector<uint32_t>& indices)
{
    MeshReport report;
    const size_t count = vertices.size() / 3;
    report.vertexCount = count;
    report.triangleCount = indices.size() / 3;

    // Per undirected edge: how many triangles use it, and the sum of their
    // directions (+1 when walked low->high, -1 when high->low). A closed,
    // consistently wound edge is walked once each way, so its balance is 0.
    struct EdgeUse {
        int uses = 0;
        int balance = 0;
    };
    std::unordered_map<uint64_t, EdgeUse> edges;
    edges.reserve(report.triangleCount * 2);

    double volumeTimesSix = 0.0;
    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        const uint32_t idx[3] = { indices[t], indices[t + 1], indices[t + 2] };
        if (idx[0] >= count || idx[1] >= count || idx[2] >= count) {
            ++report.degenerateTriangles;
            continue;
        }

        const Vec3 p0 = positionOf(vertices, idx[0]);
        const Vec3 p1 = positionOf(vertices, idx[1]);
        const Vec3 p2 = positionOf(vertices, idx[2]);
        if (length(cross(sub(p1, p0), sub(p2, p0))) < 1e-12)
            ++report.degenerateTriangles;

        // Divergence theorem: sum of signed tetrahedra from the origin.
        volumeTimesSix += dot(p0, cross(p1, p2));

        for (int e = 0; e < 3; ++e) {
            const uint32_t from = idx[e], to = idx[(e + 1) % 3];
            if (from == to) continue;
            const uint32_t lo = std::min(from, to), hi = std::max(from, to);
            EdgeUse& use = edges[(uint64_t(lo) << 32) | hi];
            ++use.uses;
            use.balance += (from < to) ? 1 : -1;
        }
    }

    for (const auto& entry : edges) {
        const EdgeUse& use = entry.second;
        if (use.uses == 1)
            ++report.boundaryEdges;
        else if (use.uses >= 3)
            ++report.nonManifoldEdges;
        else if (use.balance != 0)
            ++report.orientationErrors;
    }

    report.volume = volumeTimesSix / 6.0;
    return report;
}

bool writeBinarySTL(const std::string& path, const std::vector<float>& vertices,
                    const std::vector<uint32_t>& indices)
{
    const size_t count = vertices.size() / 3;
    if (!indicesInRange(indices, count))
        return false;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;

    char header[80] = {};
    std::strncpy(header, "kernelCAD binary STL", sizeof(header) - 1);
    out.write(header, sizeof(header));

    // Binary STL stores little-endian fields; kernelCAD runs on little-endian hosts only.
    const uint32_t triangleCount = static_cast<uint32_t>(indices.size() / 3);
    out.write(reinterpret_cast<const char*>(&triangleCount), sizeof(triangleCount));

    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        const Vec3 p0 = positionOf(vertices, indices[t]);
        const Vec3 p1 = positionOf(vertices, indices[t + 1]);
        const Vec3 p2 = positionOf(vertices, indices[t + 2]);

        Vec3 n = cross(sub(p1, p0), sub(p2, p0));
        const double len = length(n);
        n = (len > 0.0) ? Vec3{ n.x / len, n.y / len, n.z / len } : Vec3{ 0.0, 0.0, 0.0 };

        const float record[12] = {
            float(n.x),  float(n.y),  float(n.z),
            float(p0.x), float(p0.y), float(p0.z),
            float(p1.x), float(p1.y), float(p1.z),
            float(p2.x), float(p2.y), float(p2.z),
        };
        char facet[50] = {};  // 12 floats, then a 2-byte attribute count of zero
        std::memcpy(facet, record, sizeof(record));
        out.write(facet, sizeof(facet));
    }

    out.flush();
    return out.good();
}

bool writeOBJ(const std::string& path, const std::vector<float>& vertices,
              const std::vector<uint32_t>& indices)
{
    const size_t count = vertices.size() / 3;
    if (!indicesInRange(indices, count))
        return false;

    std::ofstream out(path, std::ios::trunc);
    if (!out)
        return false;

    out << "# kernelCAD OBJ export (units: mm)\n";
    char line[160];
    for (size_t i = 0; i < count; ++i) {
        std::snprintf(line, sizeof(line), "v %.6f %.6f %.6f\n",
                      double(vertices[3 * i]), double(vertices[3 * i + 1]),
                      double(vertices[3 * i + 2]));
        out << line;
    }
    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        out << "f " << indices[t] + 1 << ' ' << indices[t + 1] + 1 << ' '
            << indices[t + 2] + 1 << '\n';
    }

    out.flush();
    return out.good();
}

} // namespace kernel::trimesh
