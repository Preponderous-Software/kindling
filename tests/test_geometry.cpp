// The render geometry, without an engine: triangles face outward, every part
// of the room builds, and picked triangles map back to the right targets.
#include <cmath>

#include "bridge/room.h"
#include "check.h"

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

    // Snow disappears once shovelled out.
    changed.snow = 0;
    CHECK(buildPart(Part::Snow, changed).empty());

    // Debris pieces are built round the origin and placed by their transform.
    float model[16];
    debrisTransform(w.debris[0], model);
    CHECK(model[12] == w.debris[0].pos.x && model[14] == w.debris[0].pos.z && model[15] == 1.0f);
    CHECK(std::fabs(model[0] * model[0] + model[2] * model[2] - 1.0f) < 1e-5f);
    CHECK(hasTag(buildDebris(w.debris[14], 14), TargetKind::Debris, 14));

    TEST_MAIN_END();
}
