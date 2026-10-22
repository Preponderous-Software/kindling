// Building triangle meshes for the render bridge, with no engine involved:
// boxes, logs, cones and rods, wound counter-clockwise seen from outside (as
// Strata expects), each triangle range tagged with the target it belongs to so
// a picked triangle can be turned back into "a rotten floorboard".
//
// Colours are linear RGB, Strata's colour space.
#pragma once

#include <cstdint>
#include <vector>

#include "sim/math.h"
#include "sim/target.h"

namespace kindling::bridge {

struct Rgb {
    float r = 1, g = 1, b = 1;
};

// A run of triangles [first, first + count) that all belong to one target.
struct TriTag {
    uint32_t first = 0, count = 0;
    TargetKind kind = TargetKind::None;
    int32_t index = -1;
};

struct MeshData {
    std::vector<float> positions;  // xyz per vertex
    std::vector<float> colors;     // rgb per vertex
    std::vector<float> uvs;        // uv per vertex (filled only when wantUv)
    std::vector<uint32_t> indices;
    std::vector<TriTag> tags;
    bool wantUv = false;

    uint32_t vertexCount() const { return static_cast<uint32_t>(positions.size() / 3); }
    uint32_t triangleCount() const { return static_cast<uint32_t>(indices.size() / 3); }
    bool empty() const { return indices.empty(); }

    // Everything added between beginTag and endTag belongs to (kind, index).
    void beginTag(TargetKind kind, int32_t index = -1);
    void endTag();
    // The target a triangle belongs to (None when untagged).
    TriTag tagOf(uint32_t triangle) const;

    uint32_t vertex(Vec3 p, Rgb c, float u = 0, float v = 0);
    // Adds triangle (a, b, c), flipped if needed so it faces `outward`.
    void triangle(uint32_t a, uint32_t b, uint32_t c, Vec3 outward);
    void quad(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Rgb c, Vec3 outward);
    void box(Vec3 lo, Vec3 hi, Rgb c);
    // A horizontal log (cylinder) along x or z, from a0 to a1, centred at
    // (y, other axis), with end caps in `endColor`.
    void logX(float x0, float x1, float y, float z, float r, Rgb c, Rgb endColor, int segments = 12);
    void logZ(float z0, float z1, float y, float x, float r, Rgb c, Rgb endColor, int segments = 12);
    // A cone standing on the plane y, apex up.
    void cone(Vec3 base, float r, float h, Rgb c, Rgb tip, int segments = 10);
    // A cylinder between two arbitrary points, with flat caps.
    void rod(Vec3 a, Vec3 b, float r, Rgb c, int segments = 10);
    // A flat elliptical disc facing up at height y (a rug's rings).
    void disc(Vec3 centre, float rx, float rz, Rgb c, int segments = 28);
    // An ellipsoid (a snow drift, a tree crown, a horse's belly).
    void ellipsoid(Vec3 centre, Vec3 radii, Rgb c, int slices = 14, int stacks = 8);

    void append(const MeshData& other);

private:
    uint32_t tagStart_ = 0;
    TargetKind tagKind_ = TargetKind::None;
    int32_t tagIndex_ = -1;
    bool tagOpen_ = false;
};

// Deterministic per-call colour variation, so walls are not one flat colour.
Rgb vary(Rgb c, uint32_t key, float amount = 0.12f);

}  // namespace kindling::bridge
