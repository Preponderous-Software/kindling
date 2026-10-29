// The render geometry, without an engine: triangles face outward, every part
// of the room builds, and picked triangles map back to the right targets.
#include <cmath>
#include <cstdlib>

#include "bridge/room.h"
#include "check.h"
#include "sim/layout.h"

using namespace kindling;
using namespace kindling::bridge;

namespace {

Vec3 at(const MeshData& m, uint32_t i) { return {m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]}; }

// Every triangle of a box faces away from the box's centre.
bool facesOutward(const MeshData& m, Vec3 centre) {
    for (uint32_t t = 0; t < m.triangleCount(); ++t) {
        const Vec3 a = at(m, m.indices[3 * t]), b = at(m, m.indices[3 * t + 1]), c = at(m, m.indices[3 * t + 2]);
        const Vec3 u = b - a, v = c - a;
        const Vec3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
        const Vec3 mid = (a + b + c) * (1.0f / 3.0f);
        if (dot(n, mid - centre) <= 0) return false;
    }
    return true;
}

bool hasTag(const MeshData& m, TargetKind kind, int32_t index) {
    for (const TriTag& t : m.tags)
        if (t.kind == kind && t.index == index) return true;
    return false;
}

}  // namespace

int main() {
    {
        MeshData m;
        m.box({0, 0, 0}, {1, 2, 3}, {});
        CHECK(m.triangleCount() == 12);
        CHECK(facesOutward(m, {0.5f, 1, 1.5f}));
    }
    {
        MeshData m;
        m.logX(-1, 1, 0, 0, 0.2f, {}, {});
        CHECK(facesOutward(m, {0, 0, 0}));
        MeshData r;
        r.rod({0, 0, 0}, {0, 2, 0}, 0.3f, {});
        CHECK(facesOutward(r, {0, 1, 0}));
        MeshData e;
        e.ellipsoid({0, 0, 0}, {1, 0.5f, 1}, {});
        CHECK(facesOutward(e, {0, 0, 0}));
    }

    // Tags cover exactly the triangles added between beginTag and endTag.
    {
        MeshData m;
        m.box({0, 0, 0}, {1, 1, 1}, {});
        m.beginTag(TargetKind::Chair);
        m.box({2, 0, 0}, {3, 1, 1}, {});
        m.endTag();
        CHECK(m.tagOf(0).kind == TargetKind::None);
        CHECK(m.tagOf(12).kind == TargetKind::Chair);
        CHECK(m.tagOf(23).kind == TargetKind::Chair);

        MeshData both;
        both.box({0, 0, 0}, {1, 1, 1}, {});
        both.append(m);
        CHECK(both.tagOf(24).kind == TargetKind::Chair);  // shifted past the first box
        CHECK(both.indices[36] == 24u);  // rebased past the first box's 24 vertices
    }

    // Every part of the ruined room builds, and carries the targets the survey needs.
    const World w = newWorld(1881);
    for (int p = 0; p < static_cast<int>(Part::Count); ++p) {
        const MeshData m = buildPart(static_cast<Part>(p), w);
        CHECK(!m.empty());
        CHECK(m.positions.size() == m.colors.size());
        bool inRange = true;
        for (uint32_t i : m.indices) inRange = inRange && i < m.vertexCount();
        CHECK(inRange);
    }
    const MeshData floorMesh = buildPart(Part::Floor, w);
    for (int i = 0; i < 3; ++i) CHECK(hasTag(floorMesh, TargetKind::RottenPlank, i));
    const MeshData boardMesh = buildPart(Part::Boards, w);
    for (int i = 0; i < 4; ++i) CHECK(hasTag(boardMesh, TargetKind::WindowBoard, i));
    CHECK(hasTag(buildPart(Part::WallLeft, w), TargetKind::Wall, 1));
    CHECK(buildPart(Part::WallLeft, w).uvs.size() == buildPart(Part::WallLeft, w).vertexCount() * 2);
    CHECK(buildPart(Part::Boards, w).uvs.empty());
    CHECK(hasTag(buildPart(Part::Chimney, w), TargetKind::Firebox, -1));

    // Changes show up in the version key of the part they affect, and only there.
    World changed = w;
    changed.windowBoards[2] = 0;
    CHECK(partVersion(Part::Boards, changed) != partVersion(Part::Boards, w));
    CHECK(partVersion(Part::Floor, changed) == partVersion(Part::Floor, w));
    changed.planks[1] = PlankState::New;
    CHECK(partVersion(Part::Floor, changed) != partVersion(Part::Floor, w));
    CHECK(!hasTag(buildPart(Part::Boards, changed), TargetKind::WindowBoard, 2));

    // Snow disappears once shovelled out: the last scrap of a drift and none
    // at all have different versions, so the mesh is rebuilt (#18).
    World scrap = w;
    scrap.snow = 0.02f;
    changed.snow = 0;
    CHECK(buildPart(Part::Snow, changed).empty());
    CHECK(partVersion(Part::Snow, scrap) != partVersion(Part::Snow, changed));
    CHECK(partVersion(Part::Snow, scrap) != partVersion(Part::Snow, w));

    // Every tool but none has something to hold.
    for (int t = 1; t < kToolCount; ++t) CHECK(!buildTool(static_cast<Tool>(t)).empty());
    CHECK(buildTool(Tool::None).empty());

    // A board's pick capsule lies along it, on the floor, round its centre.
    Vec3 ca, cb;
    float cr;
    debrisCapsule(w.debris[0], ca, cb, cr);
    CHECK(std::fabs((ca.x + cb.x) / 2 - w.debris[0].pos.x) < 1e-4f && std::fabs((ca.z + cb.z) / 2 - w.debris[0].pos.z) < 1e-4f);
    CHECK(ca.y == w.debris[0].pos.y && cr > 0.1f);

    // Debris pieces are built round the origin and placed by their transform.
    float model[16];
    debrisTransform(w.debris[0], model);
    CHECK(model[12] == w.debris[0].pos.x && model[14] == w.debris[0].pos.z && model[15] == 1.0f);
    CHECK(std::fabs(model[0] * model[0] + model[2] * model[2] - 1.0f) < 1e-5f);
    CHECK(hasTag(buildDebris(w.debris[14], 14), TargetKind::Debris, 14));

    // ---- grime shows on the log walls (#41). A texel that counts as clean
    // shows the logs as built; one still to scrub is a pale, grey film over
    // them, far enough from the bare logs to see in a dim room, and it stays
    // that way until it counts as clean.
    {
        auto lum = [](const float c[3]) { return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]; };
        auto shown = [](Surface s, uint8_t g, int x, int y, Rgb base, float out[3]) {
            uint8_t t[4];
            grimeTexel(s, g, x, y, t);
            out[0] = base.r * t[0] / 255.0f;
            out[1] = base.g * t[1] / 255.0f;
            out[2] = base.b * t[2] / 255.0f;
        };
        const Rgb log{0.33f, 0.20f, 0.10f};  // the log colour (room.cpp)
        const Rgb under{log.r / kWallScrubbed.r, log.g / kWallScrubbed.g, log.b / kWallScrubbed.b};
        for (Surface s : {Surface::BackWall, Surface::LeftWall, Surface::RightWall, Surface::FrontWall})
            for (int x = 0; x < 6; ++x) {
                float bare[3], barely[3], filthy[3];
                shown(s, 0, x, 3, under, bare);
                CHECK(std::fabs(bare[0] - log.r) < 0.005f && std::fabs(bare[1] - log.g) < 0.005f &&
                      std::fabs(bare[2] - log.b) < 0.005f);
                shown(s, kCleanThreshold, x, 3, under, bare);
                CHECK(std::fabs(bare[0] - log.r) < 0.005f);
                shown(s, kCleanThreshold + 1, x, 3, under, barely);
                shown(s, 230, x, 3, under, filthy);
                for (const float* d : {barely, filthy}) {
                    CHECK(lum(d) > 1.25f * lum(bare));                    // paler
                    CHECK(d[0] / d[2] < 0.7f * (bare[0] / bare[2]));     // and greyer
                }
            }
        // The floor and the chimney look as they did: swept boards are kFloorSwept, clean stones bare.
        uint8_t t[4];
        grimeTexel(Surface::Floor, 0, 5, 5, t);
        CHECK(std::abs(t[0] - static_cast<int>(255 * kFloorSwept.r + 0.5f)) <= 1 &&
              std::abs(t[2] - static_cast<int>(255 * kFloorSwept.b + 0.5f)) <= 1);
        grimeTexel(Surface::HearthFace, 0, 5, 5, t);
        CHECK(t[0] == 255 && t[1] == 255 && t[2] == 255);
        grimeTexel(Surface::HearthFace, 255, 5, 5, t);
        CHECK(t[0] == 56);
        // The walls are built under the scrubbed tint, brighter than the logs they show.
        const MeshData back = buildPart(Part::WallBack, newWorld(1));
        CHECK(!back.colors.empty() && back.colors[0] > log.r);
    }

    // ---- a hung sampler sits flat on the face of the wall, wherever on a log
    // the crosshair met it (#40). In the groove between two logs the point is
    // 0.1 m behind their faces; the frame (0.05 m deep behind its face, its
    // cloth 0.02 m in front) went there and sank into the logs above and below.
    {
        namespace L = kindling::layout;
        struct Hang {
            int32_t wall;
            Vec3 groove;
            float face;  // the wall's inside face along its normal axis
            Vec3 normal;
        };
        const Hang hangs[4] = {
            {0, {2.4f, 1.55f, L::kInnerZ0 - 0.1f}, L::kInnerZ0, {0, 0, 1}},
            {1, {L::kInnerX0 - 0.1f, 1.55f, 2.0f}, L::kInnerX0, {1, 0, 0}},
            {2, {L::kInnerX1 + 0.1f, 1.55f, 1.0f}, L::kInnerX1, {-1, 0, 0}},
            {3, {-2.0f, 1.55f, L::kInnerZ1 + 0.1f}, L::kInnerZ1, {0, 0, -1}},
        };
        const World fresh = newWorld(5);
        for (const Hang& h : hangs) {
            Aim aim;
            aim.kind = TargetKind::Wall;
            aim.index = h.wall;
            aim.point = h.groove;
            aim.distance = 0.8f;
            Pose pose;
            CHECK(placementFor(fresh, ItemKind::Sampler, aim, 0.26f, pose));
            const Vec3 at = hungPosition(ItemKind::Sampler, pose.pos, pose.yaw);
            const float depth = h.normal.x != 0 ? at.x : at.z, sign = h.normal.x + h.normal.z;
            CHECK(std::fabs((depth - h.face) * sign - kWallHangOut) < 1e-4f);  // out from the face, not in the groove
            CHECK(std::fabs(at.y - h.groove.y) < 1e-4f);
            // The frame's back (0.05 m behind its face, along its local -z) only just meets the logs.
            CHECK((depth - h.face) * sign - 0.05f > -0.02f);
            // And the transform puts it there for a placed sampler.
            Item it;
            it.kind = ItemKind::Sampler;
            it.place = ItemPlace::Placed;
            it.pos = pose.pos;
            it.yaw = pose.yaw;
            float m[16];
            itemTransform(it, m);
            CHECK(std::fabs(m[12] - at.x) < 1e-5f && std::fabs(m[13] - at.y) < 1e-5f && std::fabs(m[14] - at.z) < 1e-5f);
            // Its face (local +z, the third column) points into the room.
            CHECK(m[8] * h.normal.x + m[10] * h.normal.z > 0.999f);
        }
        // A 0.1.2 save keeps the point the crosshair met: it is drawn on the face too.
        CHECK(std::fabs(hungPosition(ItemKind::Sampler, {L::kInnerX1 + 0.07f, 1.8f, 0}, -1.5708f).x -
                        (L::kInnerX1 - kWallHangOut)) < 1e-5f);
        // Clear of the corners and the chimney breast, and above the mantel on the breast.
        CHECK(hungPosition(ItemKind::Sampler, {L::kInnerX1, 1.8f, L::kInnerZ1 - 0.1f}, -1.5708f).z <=
              L::kInnerZ1 - kWallHangHalfWidth + 1e-4f);
        CHECK(hungPosition(ItemKind::Sampler, {L::kBreastX1 + 0.1f, 1.8f, L::kInnerZ0}, 0).x >=
              L::kBreastX1 + kWallHangHalfWidth - 1e-4f);
        const Vec3 breast = hungPosition(ItemKind::Sampler, {-0.7f, 1.6f, L::kBreastFront + 0.03f}, 0);
        CHECK(breast.z >= L::kBreastFront + 0.08f + kWallHangOut - 1e-4f);
        CHECK(breast.y - 0.36f > L::kMantelY + 0.32f);
        // Nothing else moves.
        const Vec3 rug{1.0f, L::kFloorY, 0.5f};
        const Vec3 same = hungPosition(ItemKind::Rug, rug, 0.3f);
        CHECK(same.x == rug.x && same.y == rug.y && same.z == rug.z);
    }

    TEST_MAIN_END();
}
