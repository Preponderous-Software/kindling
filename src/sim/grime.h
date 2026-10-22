// Grime: how dirty each patch of the cabin's surfaces is.
//
// Each surface (the floor, four walls, the chimney's face) is a flat grid of
// grime values, 0 clean to 255 filthy, laid over a plane in the room. Tools
// clean a disc of it around the point the crosshair is on. The renderer shows
// the same grid as a texture that darkens the surface, so what you scrub is
// exactly what comes clean.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "sim/math.h"

namespace kindling {

enum class Surface : int32_t { Floor = 0, BackWall, LeftWall, RightWall, FrontWall, HearthFace, Count };
constexpr int kSurfaceCount = static_cast<int>(Surface::Count);

// Where a surface lies: texel (i, j) covers origin + u * (i / width) * uLength
// + v * (j / height) * vLength, row j = 0 first.
struct SurfaceDef {
    Vec3 origin, u, v;          // u and v are unit axes in the plane
    float uLength, vLength;     // metres
    int width, height;          // texels
};
const SurfaceDef& surfaceDef(Surface s);

// The texel under a world point, or false when the point is off the surface.
bool texelAt(Surface s, Vec3 p, int& i, int& j);

struct Grime {
    std::array<std::vector<uint8_t>, kSurfaceCount> texels;  // width * height each, row-major

    uint8_t at(Surface s, int i, int j) const;
};

// A texel rectangle that changed this tick (empty when w == 0).
struct TexelRect {
    int x = 0, y = 0, w = 0, h = 0;
    void add(int i, int j);
};

// Grime as the player finds it: heavy on the floor (with drifts of dead leaves
// in the corners and under the window), soot round the firebox, dust on the walls.
Grime newGrime(uint64_t seed);

// Cleans a disc of radius `radius` metres around `p` by up to `strength` grime
// per texel (full strength at the centre, fading to nothing at the edge).
// Returns the texels changed.
TexelRect scrub(Grime& g, Surface s, Vec3 p, float radius, int strength);

// The share of a surface's texels at or below the clean threshold, 0..1.
float cleanFraction(const Grime& g, Surface s);
constexpr uint8_t kCleanThreshold = 40;

}  // namespace kindling
