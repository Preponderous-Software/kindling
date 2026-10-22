#include "bridge/geometry.h"

#include <algorithm>
#include <cmath>

namespace kindling::bridge {

namespace {

Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 normalize(Vec3 a) {
    const float l = length(a);
    return l > 0 ? a * (1.0f / l) : a;
}
constexpr float kTau = 6.28318531f;

}  // namespace

void MeshData::beginTag(TargetKind kind, int32_t index) {
    tagStart_ = triangleCount();
    tagKind_ = kind;
    tagIndex_ = index;
    tagOpen_ = true;
}

void MeshData::endTag() {
    if (!tagOpen_) return;
    const uint32_t n = triangleCount() - tagStart_;
    if (n) tags.push_back({tagStart_, n, tagKind_, tagIndex_});
    tagOpen_ = false;
}

TriTag MeshData::tagOf(uint32_t triangle) const {
    for (const TriTag& t : tags)
        if (triangle >= t.first && triangle < t.first + t.count) return t;
    return {};
}

uint32_t MeshData::vertex(Vec3 p, Rgb c, float u, float v) {
    positions.insert(positions.end(), {p.x, p.y, p.z});
    colors.insert(colors.end(), {c.r, c.g, c.b});
    if (wantUv) uvs.insert(uvs.end(), {u, v});
    return vertexCount() - 1;
}

void MeshData::triangle(uint32_t a, uint32_t b, uint32_t c, Vec3 outward) {
    auto at = [this](uint32_t i) { return Vec3{positions[3 * i], positions[3 * i + 1], positions[3 * i + 2]}; };
    const Vec3 n = cross(at(b) - at(a), at(c) - at(a));
    if (dot(n, outward) < 0) std::swap(b, c);
    indices.insert(indices.end(), {a, b, c});
}

void MeshData::quad(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Rgb c, Vec3 outward) {
    const uint32_t a = vertex(p0, c, 0, 0), b = vertex(p1, c, 1, 0), d = vertex(p2, c, 1, 1), e = vertex(p3, c, 0, 1);
    triangle(a, b, d, outward);
    triangle(a, d, e, outward);
}

void MeshData::box(Vec3 lo, Vec3 hi, Rgb c) {
    const float x0 = lo.x, y0 = lo.y, z0 = lo.z, x1 = hi.x, y1 = hi.y, z1 = hi.z;
    quad({x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, c, {1, 0, 0});
    quad({x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, c, {-1, 0, 0});
    quad({x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, c, {0, 1, 0});
    quad({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, c, {0, -1, 0});
    quad({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, c, {0, 0, 1});
    quad({x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, c, {0, 0, -1});
}

namespace {

// A cylinder along the axis `along` (0 = x, 2 = z) with shared side vertices
// (so the engine's smooth normals round it) and flat end caps.
void cylinder(MeshData& m, int along, float a0, float a1, float y, float other, float r, Rgb c, Rgb endColor,
              int n) {
    auto at = [&](float a, float ang) {
        const float u = r * std::cos(ang), v = r * std::sin(ang);
        return along == 0 ? Vec3{a, y + v, other + u} : Vec3{other + u, y + v, a};
    };
    std::vector<uint32_t> ring0(n), ring1(n);
    for (int k = 0; k < n; ++k) {
        ring0[k] = m.vertex(at(a0, kTau * k / n), c);
        ring1[k] = m.vertex(at(a1, kTau * k / n), c);
    }
    for (int k = 0; k < n; ++k) {
        const int k2 = (k + 1) % n;
        const float mid = kTau * (k + 0.5f) / n;
        const Vec3 out = along == 0 ? Vec3{0, std::sin(mid), std::cos(mid)} : Vec3{std::cos(mid), std::sin(mid), 0};
        m.triangle(ring0[k], ring1[k], ring1[k2], out);
        m.triangle(ring0[k], ring1[k2], ring0[k2], out);
    }
    for (int end = 0; end < 2; ++end) {
        const float a = end ? a1 : a0;
        const float sign = end ? 1.0f : -1.0f;
        const Vec3 out = along == 0 ? Vec3{sign, 0, 0} : Vec3{0, 0, sign};
        const uint32_t centre = m.vertex(along == 0 ? Vec3{a, y, other} : Vec3{other, y, a}, endColor);
        std::vector<uint32_t> rim(n);
        for (int k = 0; k < n; ++k) rim[k] = m.vertex(at(a, kTau * k / n), endColor);
        for (int k = 0; k < n; ++k) m.triangle(centre, rim[k], rim[(k + 1) % n], out);
    }
}

}  // namespace

void MeshData::logX(float x0, float x1, float y, float z, float r, Rgb c, Rgb endColor, int segments) {
    cylinder(*this, 0, x0, x1, y, z, r, c, endColor, segments);
}

void MeshData::logZ(float z0, float z1, float y, float x, float r, Rgb c, Rgb endColor, int segments) {
    cylinder(*this, 2, z0, z1, y, x, r, c, endColor, segments);
}

void MeshData::cone(Vec3 base, float r, float h, Rgb c, Rgb tip, int n) {
    const uint32_t apex = vertex({base.x, base.y + h, base.z}, tip);
    std::vector<uint32_t> rim(n);
    for (int k = 0; k < n; ++k)
        rim[k] = vertex({base.x + r * std::cos(kTau * k / n), base.y, base.z + r * std::sin(kTau * k / n)}, c);
    for (int k = 0; k < n; ++k) {
        const float mid = kTau * (k + 0.5f) / n;
        triangle(apex, rim[k], rim[(k + 1) % n], {std::cos(mid), 0.3f, std::sin(mid)});
    }
}

void MeshData::rod(Vec3 a, Vec3 b, float r, Rgb c, int n) { frustum(a, b, r, r, c, n); }

void MeshData::frustum(Vec3 a, Vec3 b, float ra, float rb, Rgb c, int n) {
    const Vec3 axis = normalize(b - a);
    const Vec3 t = std::fabs(axis.y) < 0.9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    const Vec3 u = normalize(cross(axis, t)), v = cross(axis, u);
    std::vector<uint32_t> r0(n), r1(n);
    for (int k = 0; k < n; ++k) {
        const float ang = kTau * k / n;
        const Vec3 dir = u * std::cos(ang) + v * std::sin(ang);
        r0[k] = vertex(a + dir * ra, c);
        r1[k] = vertex(b + dir * rb, c);
    }
    for (int k = 0; k < n; ++k) {
        const int k2 = (k + 1) % n;
        const float mid = kTau * (k + 0.5f) / n;
        // Outward, tilted by the taper.
        const Vec3 out = u * std::cos(mid) + v * std::sin(mid) + axis * ((ra - rb) / std::max(1e-4f, length(b - a)));
        triangle(r0[k], r1[k], r1[k2], out);
        triangle(r0[k], r1[k2], r0[k2], out);
    }
    for (int end = 0; end < 2; ++end) {
        const Vec3 centre = end ? b : a;
        const float r = end ? rb : ra;
        const Vec3 out = end ? axis : axis * -1.0f;
        const uint32_t ci = vertex(centre, c);
        std::vector<uint32_t> rim(n);
        for (int k = 0; k < n; ++k) {
            const float ang = kTau * k / n;
            rim[k] = vertex(centre + u * (r * std::cos(ang)) + v * (r * std::sin(ang)), c);
        }
        for (int k = 0; k < n; ++k) triangle(ci, rim[k], rim[(k + 1) % n], out);
    }
}

void MeshData::tube(const std::vector<Vec3>& path, float r, Rgb c, int n) {
    if (path.size() < 2) return;
    // One ring per path point, oriented along the path there.
    std::vector<std::vector<uint32_t>> rings;
    std::vector<Vec3> dirs;
    Vec3 up{0, 1, 0};
    for (size_t i = 0; i < path.size(); ++i) {
        const Vec3 d = normalize(path[std::min(i + 1, path.size() - 1)] - path[i > 0 ? i - 1 : 0]);
        const Vec3 t = std::fabs(dot(d, up)) < 0.9f ? up : Vec3{1, 0, 0};
        const Vec3 u = normalize(cross(d, t)), v = cross(d, u);
        std::vector<uint32_t> ring(n);
        for (int k = 0; k < n; ++k) {
            const float ang = kTau * k / n;
            ring[k] = vertex(path[i] + u * (r * std::cos(ang)) + v * (r * std::sin(ang)), c);
        }
        rings.push_back(ring);
        dirs.push_back(d);
    }
    auto at = [this](uint32_t k) { return Vec3{positions[3 * k], positions[3 * k + 1], positions[3 * k + 2]}; };
    for (size_t i = 0; i + 1 < path.size(); ++i)
        for (int k = 0; k < n; ++k) {
            const int k2 = (k + 1) % n;
            const Vec3 out = at(rings[i][k]) - path[i];
            triangle(rings[i][k], rings[i + 1][k], rings[i + 1][k2], out);
            triangle(rings[i][k], rings[i + 1][k2], rings[i][k2], out);
        }
    for (int end = 0; end < 2; ++end) {
        const size_t i = end ? path.size() - 1 : 0;
        const Vec3 out = end ? dirs[i] : dirs[i] * -1.0f;
        const uint32_t ci = vertex(path[i], c);
        for (int k = 0; k < n; ++k) triangle(ci, rings[i][k], rings[i][(k + 1) % n], out);
    }
}

void MeshData::disc(Vec3 centre, float rx, float rz, Rgb c, int n) {
    const uint32_t mid = vertex(centre, c);
    std::vector<uint32_t> rim(n);
    for (int k = 0; k < n; ++k)
        rim[k] = vertex({centre.x + rx * std::cos(kTau * k / n), centre.y, centre.z + rz * std::sin(kTau * k / n)}, c);
    for (int k = 0; k < n; ++k) triangle(mid, rim[k], rim[(k + 1) % n], {0, 1, 0});
}

void MeshData::ellipsoid(Vec3 centre, Vec3 radii, Rgb c, int slices, int stacks) {
    const uint32_t base = vertexCount();
    for (int i = 0; i <= stacks; ++i) {
        const float lat = 3.14159265f * (static_cast<float>(i) / stacks - 0.5f);
        for (int j = 0; j < slices; ++j) {
            const float lon = kTau * j / slices;
            vertex({centre.x + radii.x * std::cos(lat) * std::cos(lon), centre.y + radii.y * std::sin(lat),
                    centre.z + radii.z * std::cos(lat) * std::sin(lon)},
                   c);
        }
    }
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            const int j2 = (j + 1) % slices;
            const uint32_t a = base + i * slices + j, b = base + i * slices + j2;
            const uint32_t d = base + (i + 1) * slices + j, e = base + (i + 1) * slices + j2;
            auto at = [this](uint32_t k) { return Vec3{positions[3 * k], positions[3 * k + 1], positions[3 * k + 2]}; };
            const Vec3 out = at(a) - centre;
            if (i > 0) triangle(a, b, e, out);
            if (i < stacks - 1) triangle(a, e, d, out);
        }
}

void MeshData::append(const MeshData& other) {
    const uint32_t base = vertexCount(), triBase = triangleCount();
    positions.insert(positions.end(), other.positions.begin(), other.positions.end());
    colors.insert(colors.end(), other.colors.begin(), other.colors.end());
    if (wantUv) {
        if (other.wantUv) uvs.insert(uvs.end(), other.uvs.begin(), other.uvs.end());
        else uvs.resize(positions.size() / 3 * 2, 0.0f);
    }
    for (uint32_t i : other.indices) indices.push_back(base + i);
    for (TriTag t : other.tags) {
        t.first += triBase;
        tags.push_back(t);
    }
}

Rgb vary(Rgb c, uint32_t key, float amount) {
    uint32_t h = key * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    const float f = 1.0f + amount * (static_cast<float>(h % 2001) / 1000.0f - 1.0f);
    auto cl = [](float v) { return v > 1 ? 1.0f : v; };
    return {cl(c.r * f), cl(c.g * f), cl(c.b * f)};
}

}  // namespace kindling::bridge
