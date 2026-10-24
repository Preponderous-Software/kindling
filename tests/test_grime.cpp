// Grime (src/sim/grime.h) on its own: where a point lands on a surface, how a
// scrub fades from its centre and reports what it changed, how clean a surface
// or a patch counts as, and the grime the player finds.
#include <cstddef>
#include <initializer_list>
#include <vector>

#include "check.h"
#include "sim/grime.h"
#include "sim/layout.h"

using namespace kindling;
namespace L = kindling::layout;

namespace {

// Every texel of every surface set to `v`, so a test controls what it starts from.
Grime flat(uint8_t v) {
    Grime g;
    for (int s = 0; s < kSurfaceCount; ++s) {
        const SurfaceDef& d = surfaceDef(static_cast<Surface>(s));
        g.texels[static_cast<size_t>(s)].assign(static_cast<size_t>(d.width * d.height), v);
    }
    return g;
}

// The world point at the middle of texel (i, j).
Vec3 centreOf(Surface s, int i, int j) {
    const SurfaceDef& d = surfaceDef(s);
    return d.origin + d.u * ((i + 0.5f) / d.width * d.uLength) + d.v * ((j + 0.5f) / d.height * d.vLength);
}

}  // namespace

int main() {
    // ---- the surfaces: the floor at 12 texels a metre, the walls and chimney at 8
    {
        const SurfaceDef& floor = surfaceDef(Surface::Floor);
        CHECK(floor.width == 95 && floor.height == 85);
        const SurfaceDef& back = surfaceDef(Surface::BackWall);
        CHECK(back.width == 64 && back.height == 24);
        const SurfaceDef& left = surfaceDef(Surface::LeftWall);
        CHECK(left.width == 57 && left.height == 24);
        const SurfaceDef& face = surfaceDef(Surface::HearthFace);
        CHECK(face.width == 24 && face.height == 24);
        const Grime g = newGrime(1);
        for (int s = 0; s < kSurfaceCount; ++s) {
            const SurfaceDef& d = surfaceDef(static_cast<Surface>(s));
            CHECK(g.texels[static_cast<size_t>(s)].size() == static_cast<size_t>(d.width * d.height));
        }
    }

    // ---- texelAt: corners, the far edge, off the surface
    {
        int i = -1, j = -1;
        CHECK(texelAt(Surface::Floor, {L::kInnerX0, L::kFloorY, L::kInnerZ0}, i, j));
        CHECK(i == 0 && j == 0);
        // The far corner is on the surface and lands in the last texel, not past it.
        CHECK(texelAt(Surface::Floor, {L::kInnerX1, L::kFloorY, L::kInnerZ1}, i, j));
        CHECK(i == 94 && j == 84);
        CHECK(texelAt(Surface::Floor, centreOf(Surface::Floor, 40, 50), i, j));
        CHECK(i == 40 && j == 50);
        // Off the surface: nothing written.
        i = j = -7;
        CHECK(!texelAt(Surface::Floor, {L::kInnerX0 - 0.01f, L::kFloorY, 0}, i, j));
        CHECK(!texelAt(Surface::Floor, {0, L::kFloorY, L::kInnerZ1 + 0.01f}, i, j));
        CHECK(i == -7 && j == -7);
        // A point off the plane is projected onto it: only u and v are looked at.
        CHECK(texelAt(Surface::Floor, {0, 1.5f, 0}, i, j));
        // Walls run up from y = 0 to the ceiling.
        CHECK(texelAt(Surface::BackWall, {0, 1.2f, L::kInnerZ0}, i, j));
        CHECK(!texelAt(Surface::BackWall, {0, L::kCeilingY + 0.01f, L::kInnerZ0}, i, j));
        CHECK(texelAt(Surface::RightWall, {L::kInnerX1, 0.5f, 0}, i, j));
        CHECK(!texelAt(Surface::HearthFace, {L::kBreastX1 + 0.01f, 1.0f, L::kBreastFront}, i, j));
    }

    // ---- Grime::at reads row-major, row j = 0 first
    {
        Grime g = flat(100);
        const int w = surfaceDef(Surface::Floor).width;
        g.texels[static_cast<size_t>(Surface::Floor)][static_cast<size_t>(3 * w + 7)] = 9;
        CHECK(g.at(Surface::Floor, 7, 3) == 9);
        CHECK(g.at(Surface::Floor, 3, 7) == 100);
    }

    // ---- TexelRect grows to the bounding box of the texels added
    {
        TexelRect r;
        CHECK(r.w == 0);
        r.add(5, 2);
        CHECK(r.x == 5 && r.y == 2 && r.w == 1 && r.h == 1);
        r.add(2, 7);
        CHECK(r.x == 2 && r.y == 2 && r.w == 4 && r.h == 6);
        r.add(3, 4);  // inside: no change
        CHECK(r.x == 2 && r.y == 2 && r.w == 4 && r.h == 6);
        r.add(9, 0);
        CHECK(r.x == 2 && r.y == 0 && r.w == 8 && r.h == 8);
    }

    // ---- scrub: full strength at the centre, fading out to the edge of the disc
    {
        Grime g = flat(200);
        const Grime before = g;
        const Vec3 p = centreOf(Surface::Floor, 40, 50);
        const TexelRect r = scrub(g, Surface::Floor, p, 0.3f, 100);
        CHECK(g.at(Surface::Floor, 40, 50) == 100);
        // Moving out from the centre each texel loses less, and past the radius nothing.
        int last = g.at(Surface::Floor, 40, 50);
        for (int i = 41; i <= 46; ++i) {
            CHECK(g.at(Surface::Floor, i, 50) >= last);
            last = g.at(Surface::Floor, i, 50);
        }
        CHECK(g.at(Surface::Floor, 41, 50) > 100 && g.at(Surface::Floor, 41, 50) < 200);
        CHECK(g.at(Surface::Floor, 44, 50) == 200);  // 0.33 m away, past the 0.3 m radius
        CHECK(g.at(Surface::Floor, 40, 46) == 200);
        // The rect is exactly the bounding box of the texels that changed.
        int x0 = 1000, y0 = 1000, x1 = -1, y1 = -1;
        const SurfaceDef& d = surfaceDef(Surface::Floor);
        for (int j = 0; j < d.height; ++j)
            for (int i = 0; i < d.width; ++i)
                if (g.at(Surface::Floor, i, j) != before.at(Surface::Floor, i, j)) {
                    x0 = i < x0 ? i : x0;
                    y0 = j < y0 ? j : y0;
                    x1 = i > x1 ? i : x1;
                    y1 = j > y1 ? j : y1;
                }
        CHECK(r.w > 0);
        CHECK(r.x == x0 && r.y == y0 && r.x + r.w - 1 == x1 && r.y + r.h - 1 == y1);
        // The other surfaces are untouched.
        CHECK(g.texels[static_cast<size_t>(Surface::BackWall)] == before.texels[static_cast<size_t>(Surface::BackWall)]);
    }

    // ---- scrub stops at clean, and says nothing changed once it is
    {
        Grime g = flat(200);
        const Vec3 p = centreOf(Surface::Floor, 40, 50);
        CHECK(scrub(g, Surface::Floor, p, 0.3f, 1000).w > 0);
        CHECK(g.at(Surface::Floor, 40, 50) == 0);  // not wrapped round past zero
        CHECK(g.at(Surface::Floor, 42, 50) == 0);
        // Near the rim a pass takes off less than 200, so keep going until nothing changes.
        int passes = 0;
        while (scrub(g, Surface::Floor, p, 0.3f, 1000).w > 0 && passes < 300) ++passes;
        CHECK(passes > 0 && passes < 300);
        // Every texel the disc reaches is now 0, and a texel at 0 is not reported as changed.
        CHECK(g.at(Surface::Floor, 43, 50) == 0);
        CHECK(g.at(Surface::Floor, 44, 50) == 200);  // past the rim: never touched
        const Grime done = g;
        CHECK(scrub(g, Surface::Floor, p, 0.3f, 1000).w == 0);
        CHECK(g.texels == done.texels);
    }

    // ---- scrub does nothing at no strength, or off the surface; near an edge it is clipped
    {
        Grime g = flat(200);
        const Grime before = g;
        CHECK(scrub(g, Surface::Floor, centreOf(Surface::Floor, 40, 50), 0.3f, 0).w == 0);
        CHECK(scrub(g, Surface::Floor, {L::kInnerX1 + 5.0f, L::kFloorY, 0}, 0.3f, 100).w == 0);
        CHECK(g.texels == before.texels);
        const TexelRect corner = scrub(g, Surface::Floor, {L::kInnerX0, L::kFloorY, L::kInnerZ0}, 0.3f, 100);
        CHECK(corner.w > 0 && corner.x == 0 && corner.y == 0);
        CHECK(g.at(Surface::Floor, 0, 0) < 200);
    }

    // ---- cleanFraction: the share at or below the threshold, the walls weighted by their texels
    {
        Grime g = flat(static_cast<uint8_t>(kCleanThreshold + 1));
        CHECK(cleanFraction(g, Surface::Floor) == 0.0f);
        for (uint8_t& v : g.texels[static_cast<size_t>(Surface::Floor)]) v = kCleanThreshold;
        CHECK(cleanFraction(g, Surface::Floor) == 1.0f);  // the threshold itself counts as clean
        // Half the back wall clean.
        std::vector<uint8_t>& back = g.texels[static_cast<size_t>(Surface::BackWall)];
        for (size_t k = 0; k < back.size() / 2; ++k) back[k] = 0;
        CHECK(cleanFraction(g, Surface::BackWall) == 0.5f);
        // Together with a dirty left wall: weighted by how many texels each has.
        const float both = cleanFraction(g, {Surface::BackWall, Surface::LeftWall});
        const float expected = static_cast<float>(back.size() / 2) /
                               static_cast<float>(back.size() + g.texels[static_cast<size_t>(Surface::LeftWall)].size());
        CHECK(both == expected);
        CHECK(cleanFraction(g, {Surface::Floor}) == 1.0f);
        CHECK(cleanFraction(g, std::initializer_list<Surface>{}) == 0.0f);
        // A grime with no texels at all counts as not clean.
        CHECK(cleanFraction(Grime{}, Surface::Floor) == 0.0f);
    }

    // ---- patchClean: the share of the disc that is clean, 1 off the surface
    {
        Grime g = flat(200);
        const Vec3 p = centreOf(Surface::Floor, 40, 50);
        CHECK(patchClean(g, Surface::Floor, p, 0.3f) == 0.0f);
        CHECK(patchClean(g, Surface::Floor, {L::kInnerX1 + 5.0f, L::kFloorY, 0}, 0.3f) == 1.0f);
        // Scrubbed hard over a wider disc, the smaller patch inside it is all clean.
        scrub(g, Surface::Floor, p, 0.5f, 1000);
        CHECK(patchClean(g, Surface::Floor, p, 0.3f) == 1.0f);
        // Only the middle texel at the threshold: part of the patch, not all of it.
        Grime h = flat(200);
        h.texels[static_cast<size_t>(Surface::Floor)][static_cast<size_t>(50 * surfaceDef(Surface::Floor).width + 40)] =
            kCleanThreshold;
        const float part = patchClean(h, Surface::Floor, p, 0.3f);
        CHECK(part > 0.0f && part < 0.1f);
    }

    // ---- newGrime: the same seed gives the same grime, another seed another
    {
        const Grime a = newGrime(7), b = newGrime(7), c = newGrime(8);
        CHECK(a.texels == b.texels);
        CHECK(a.texels != c.texels);
        // Every texel the player can see is dirty (60 or worse); hidden ones are 0.
        bool inRange = true;
        for (int s = 0; s < kSurfaceCount; ++s)
            for (uint8_t v : a.texels[static_cast<size_t>(s)]) inRange = inRange && (v == 0 || v >= 60);
        CHECK(inRange);
        int i, j;
        CHECK(texelAt(Surface::Floor, {-1.0f, L::kFloorY, -2.0f}, i, j));  // under the hearth
        CHECK(a.at(Surface::Floor, i, j) == 0);
        CHECK(texelAt(Surface::BackWall, {-1.0f, 1.5f, L::kInnerZ0}, i, j));  // behind the chimney
        CHECK(a.at(Surface::BackWall, i, j) == 0);
        CHECK(texelAt(Surface::BackWall, {2.5f, 1.5f, L::kInnerZ0}, i, j));
        CHECK(a.at(Surface::BackWall, i, j) >= 60);
        CHECK(texelAt(Surface::Floor, {1.5f, L::kFloorY, 2.0f}, i, j));
        CHECK(a.at(Surface::Floor, i, j) > kCleanThreshold);
        // Nothing the player can see starts clean, so the fresh floor is clean only where hidden.
        CHECK(cleanFraction(a, Surface::Floor) > 0.0f && cleanFraction(a, Surface::Floor) < 0.15f);
        CHECK(cleanFraction(a, Surface::RightWall) == 0.0f);
    }

    TEST_MAIN_END();
}
