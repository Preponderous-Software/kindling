#include "sim/grime.h"

#include <algorithm>
#include <cmath>

#include "sim/layout.h"
#include "sim/rng.h"

namespace kindling {

namespace L = kindling::layout;

namespace {

constexpr float kFloorTexelsPerMetre = 12.0f, kWallTexelsPerMetre = 8.0f;

SurfaceDef make(Vec3 origin, Vec3 u, Vec3 v, float uLen, float vLen, float perMetre) {
    return {origin, u, v, uLen, vLen, static_cast<int>(std::ceil(uLen * perMetre)),
            static_cast<int>(std::ceil(vLen * perMetre))};
}

const std::array<SurfaceDef, kSurfaceCount>& defs() {
    static const std::array<SurfaceDef, kSurfaceCount> d = [] {
        const float roomW = L::kInnerX1 - L::kInnerX0, roomD = L::kInnerZ1 - L::kInnerZ0, h = L::kCeilingY;
        std::array<SurfaceDef, kSurfaceCount> a{};
        a[0] = make({L::kInnerX0, L::kFloorY, L::kInnerZ0}, {1, 0, 0}, {0, 0, 1}, roomW, roomD, kFloorTexelsPerMetre);
        a[1] = make({L::kInnerX0, 0, L::kInnerZ0}, {1, 0, 0}, {0, 1, 0}, roomW, h, kWallTexelsPerMetre);
        a[2] = make({L::kInnerX0, 0, L::kInnerZ0}, {0, 0, 1}, {0, 1, 0}, roomD, h, kWallTexelsPerMetre);
        a[3] = make({L::kInnerX1, 0, L::kInnerZ0}, {0, 0, 1}, {0, 1, 0}, roomD, h, kWallTexelsPerMetre);
        a[4] = make({L::kInnerX0, 0, L::kInnerZ1}, {1, 0, 0}, {0, 1, 0}, roomW, h, kWallTexelsPerMetre);
        a[5] = make({L::kBreastX0, 0, L::kBreastFront}, {1, 0, 0}, {0, 1, 0}, L::kBreastX1 - L::kBreastX0, h,
                    kWallTexelsPerMetre);
        return a;
    }();
    return d;
}

// Smooth value noise in [0, 1] from a small random lattice, for drifts and patches.
struct Noise {
    explicit Noise(Rng& rng) {
        for (float& v : lattice) v = rng.uniform(0, 1);
    }
    float at(float x, float y) const {
        const float fx = std::floor(x), fy = std::floor(y);
        const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
        const float tx = x - fx, ty = y - fy;
        auto l = [this](int i, int j) { return lattice[((i & 15) * 16 + (j & 15))]; };
        const float a = l(ix, iy) + (l(ix + 1, iy) - l(ix, iy)) * tx;
        const float b = l(ix, iy + 1) + (l(ix + 1, iy + 1) - l(ix, iy + 1)) * tx;
        return a + (b - a) * ty;
    }
    float lattice[256];
};

}  // namespace

const SurfaceDef& surfaceDef(Surface s) { return defs()[static_cast<size_t>(s)]; }

bool texelAt(Surface s, Vec3 p, int& i, int& j) {
    const SurfaceDef& d = surfaceDef(s);
    const Vec3 r = p - d.origin;
    const float u = dot(r, d.u) / d.uLength, v = dot(r, d.v) / d.vLength;
    if (u < 0 || u > 1 || v < 0 || v > 1) return false;
    i = std::min(d.width - 1, static_cast<int>(u * d.width));
    j = std::min(d.height - 1, static_cast<int>(v * d.height));
    return true;
}

uint8_t Grime::at(Surface s, int i, int j) const {
    const SurfaceDef& d = surfaceDef(s);
    return texels[static_cast<size_t>(s)][static_cast<size_t>(j * d.width + i)];
}

void TexelRect::add(int i, int j) {
    if (w == 0) {
        x = i;
        y = j;
        w = h = 1;
        return;
    }
    const int x1 = std::max(x + w, i + 1), y1 = std::max(y + h, j + 1);
    x = std::min(x, i);
    y = std::min(y, j);
    w = x1 - x;
    h = y1 - y;
}

Grime newGrime(uint64_t seed) {
    Grime g;
    Rng rng(seed ^ 0x9e3779b97f4a7c15ULL);
    const Noise noise(rng);
    for (int s = 0; s < kSurfaceCount; ++s) {
        const SurfaceDef& d = defs()[static_cast<size_t>(s)];
        std::vector<uint8_t>& t = g.texels[static_cast<size_t>(s)];
        t.resize(static_cast<size_t>(d.width * d.height));
        for (int j = 0; j < d.height; ++j)
            for (int i = 0; i < d.width; ++i) {
                const Vec3 p = d.origin + d.u * ((i + 0.5f) / d.width * d.uLength) +
                               d.v * ((j + 0.5f) / d.height * d.vLength);
                const float n = noise.at(p.x * 1.3f + p.y * 0.7f, p.z * 1.3f + p.y * 0.9f);
                float grime;
                if (s == static_cast<int>(Surface::Floor)) {
                    // Leaves drift into the corners and pile under the window.
                    const float edge = std::min({p.x - L::kInnerX0, L::kInnerX1 - p.x, p.z - L::kInnerZ0,
                                                 L::kInnerZ1 - p.z});
                    const float drift = std::max(0.0f, 1.0f - edge / 1.2f);
                    const float underWindow = (p.x < L::kInnerX0 + 1.0f && p.z > L::kWindowZ0 - 0.6f &&
                                               p.z < L::kWindowZ1 + 0.6f) ? 0.4f : 0.0f;
                    grime = 170 + 60 * n + 40 * drift + 60 * underWindow + rng.uniform(-12, 12);
                } else if (s == static_cast<int>(Surface::HearthFace)) {
                    // Soot fans up from the firebox opening.
                    const float dx = std::fabs(p.x - (L::kFireboxX0 + L::kFireboxX1) / 2);
                    const float soot = std::max(0.0f, 1.0f - dx / 1.1f) * std::max(0.0f, 1.0f - std::fabs(p.y - 1.3f) / 1.4f);
                    grime = 120 + 50 * n + 110 * soot + rng.uniform(-10, 10);
                } else {
                    // Walls: dust, worst low down and in the corners.
                    grime = 110 + 70 * n + 50 * std::max(0.0f, 1.0f - p.y / 1.2f) + rng.uniform(-10, 10);
                }
                // Hidden behind or under the chimney: never seen, never counted against the player.
                const bool underChimney = s == static_cast<int>(Surface::Floor) && p.x > -2.55f && p.x < 0.95f &&
                                          p.z < L::kHearthstoneZ1;
                const bool behindChimney = s == static_cast<int>(Surface::BackWall) && p.x > L::kBreastX0 &&
                                           p.x < L::kBreastX1;
                t[static_cast<size_t>(j * d.width + i)] =
                    underChimney || behindChimney ? 0 : static_cast<uint8_t>(std::clamp(grime, 60.0f, 255.0f));
            }
    }
    return g;
}

TexelRect scrub(Grime& g, Surface s, Vec3 p, float radius, int strength) {
    TexelRect changed;
    const SurfaceDef& d = surfaceDef(s);
    const Vec3 r = p - d.origin;
    const float cu = dot(r, d.u) / d.uLength * d.width, cv = dot(r, d.v) / d.vLength * d.height;
    const float ru = radius / d.uLength * d.width, rv = radius / d.vLength * d.height;
    const int i0 = std::max(0, static_cast<int>(std::floor(cu - ru))), i1 = std::min(d.width - 1, static_cast<int>(cu + ru));
    const int j0 = std::max(0, static_cast<int>(std::floor(cv - rv))), j1 = std::min(d.height - 1, static_cast<int>(cv + rv));
    std::vector<uint8_t>& t = g.texels[static_cast<size_t>(s)];
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i) {
            const float du = (i + 0.5f - cu) / ru, dv = (j + 0.5f - cv) / rv;
            const float dist = std::sqrt(du * du + dv * dv);
            if (dist >= 1) continue;
            const int amount = static_cast<int>(static_cast<float>(strength) * (1.0f - dist) + 0.5f);
            uint8_t& texel = t[static_cast<size_t>(j * d.width + i)];
            if (amount <= 0 || texel == 0) continue;
            texel = static_cast<uint8_t>(std::max(0, texel - amount));
            changed.add(i, j);
        }
    return changed;
}

float cleanFraction(const Grime& g, Surface s) {
    const std::vector<uint8_t>& t = g.texels[static_cast<size_t>(s)];
    if (t.empty()) return 0;
    size_t clean = 0;
    for (uint8_t v : t) clean += v <= kCleanThreshold;
    return static_cast<float>(clean) / static_cast<float>(t.size());
}

float cleanFraction(const Grime& g, std::initializer_list<Surface> surfaces) {
    size_t clean = 0, total = 0;
    for (Surface s : surfaces) {
        for (uint8_t v : g.texels[static_cast<size_t>(s)]) clean += v <= kCleanThreshold;
        total += g.texels[static_cast<size_t>(s)].size();
    }
    return total ? static_cast<float>(clean) / static_cast<float>(total) : 0;
}

float patchClean(const Grime& g, Surface s, Vec3 p, float radius) {
    const SurfaceDef& d = surfaceDef(s);
    const Vec3 r = p - d.origin;
    const float cu = dot(r, d.u) / d.uLength * d.width, cv = dot(r, d.v) / d.vLength * d.height;
    const float ru = radius / d.uLength * d.width, rv = radius / d.vLength * d.height;
    const int i0 = std::max(0, static_cast<int>(std::floor(cu - ru))), i1 = std::min(d.width - 1, static_cast<int>(cu + ru));
    const int j0 = std::max(0, static_cast<int>(std::floor(cv - rv))), j1 = std::min(d.height - 1, static_cast<int>(cv + rv));
    int clean = 0, total = 0;
    for (int j = j0; j <= j1; ++j)
        for (int i = i0; i <= i1; ++i) {
            const float du = (i + 0.5f - cu) / ru, dv = (j + 0.5f - cv) / rv;
            if (du * du + dv * dv >= 1) continue;
            ++total;
            clean += g.at(s, i, j) <= kCleanThreshold;
        }
    return total ? static_cast<float>(clean) / static_cast<float>(total) : 1.0f;
}

}  // namespace kindling
