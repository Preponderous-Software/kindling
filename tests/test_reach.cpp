// Reach and carrying: how far a target can be worked or picked up, whether the
// tool in hand has work to do on a target however far off it is (for "too far
// to reach"), and where a carried thing rides in view.
#include <cmath>

#include "check.h"
#include "sim/layout.h"
#include "sim/rules.h"

using namespace kindling;
namespace L = kindling::layout;

namespace {

Aim aimAt(TargetKind kind, int32_t index, float distance) {
    Aim a;
    a.kind = kind;
    a.index = index;
    a.distance = distance;
    return a;
}

}  // namespace

int main() {
    // ---- reachFor: debris can be picked up from a little further off
    {
        CHECK(reachFor(TargetKind::Debris) == L::kDebrisReach);
        CHECK(reachFor(TargetKind::Floor) == L::kReach);
        CHECK(reachFor(TargetKind::Item) == L::kReach);
        CHECK(reachFor(TargetKind::None) == L::kReach);
        CHECK(L::kDebrisReach > L::kReach);
    }

    // ---- inReach: nothing aimed at is never in reach; the reach is inclusive
    {
        const World w = newWorld(1);
        CHECK(!inReach(w, aimAt(TargetKind::None, -1, 0)));
        CHECK(inReach(w, aimAt(TargetKind::Floor, -1, L::kReach)));
        CHECK(!inReach(w, aimAt(TargetKind::Floor, -1, L::kReach + 0.01f)));
        // Between the two reaches: debris yes, anything else no.
        const float between = (L::kReach + L::kDebrisReach) / 2;
        CHECK(inReach(w, aimAt(TargetKind::Debris, 0, between)));
        CHECK(!inReach(w, aimAt(TargetKind::Wall, 0, between)));
        CHECK(!inReach(w, aimAt(TargetKind::Debris, 0, L::kDebrisReach + 0.01f)));
    }

    // ---- toolApplies: work to do with the tool in hand, near or far
    {
        World w = newWorld(1);
        const Aim farSnow = aimAt(TargetKind::Snow, -1, 20.0f);
        w.tool = Tool::Shovel;
        CHECK(w.snow > 0);
        CHECK(toolApplies(w, farSnow));     // too far to work, but there is work
        CHECK(workMeter(w, farSnow) < 0);   // and so no meter at the crosshair
        w.snow = 0.5f;
        CHECK(toolApplies(w, farSnow));
        w.snow = 0;
        CHECK(!toolApplies(w, farSnow));    // nothing left to shovel

        w.snow = 1.0f;
        w.tool = Tool::Broom;
        CHECK(!toolApplies(w, farSnow));    // the wrong tool
        w.tool = Tool::None;
        CHECK(!toolApplies(w, farSnow));
        CHECK(!toolApplies(w, aimAt(TargetKind::None, -1, 1.0f)));

        // Carrying debris, no tool works on anything.
        w.tool = Tool::Shovel;
        w.carry = Carry::Debris;
        CHECK(!toolApplies(w, farSnow));
    }

    // A timed job that is already finished has no work left: its meter is full.
    {
        World w = newWorld(1);
        w.tool = Tool::Hammer;
        const Aim chair = aimAt(TargetKind::Chair, -1, 1.0f);
        CHECK(w.chair == ChairState::Broken);
        CHECK(toolApplies(w, chair));
        w.workKind = TargetKind::Chair;
        w.workIndex = -1;
        w.workSeconds = 1000.0f;
        CHECK(workMeter(w, chair) >= 1.0f);
        CHECK(!toolApplies(w, chair));
        w.chair = ChairState::Mended;
        w.workSeconds = 0;
        CHECK(!toolApplies(w, chair));
    }

    // ---- carryPosition: low and to the right of the view, wherever it points
    {
        Player p;
        p.x = 1.0f;
        p.z = 2.0f;
        p.yaw = 0;  // facing -z; the right hand is +x
        p.pitch = 0;
        const Vec3 c = carryPosition(p);
        CHECK(c.x > p.x + 0.2f);
        CHECK(c.z < p.z - 0.3f);
        CHECK(c.y < L::kEyeHeight - 0.2f && c.y > L::kFloorY);

        p.yaw = 1.5707963f;  // facing +x; the right hand is +z
        const Vec3 d = carryPosition(p);
        CHECK(d.x > p.x + 0.3f);
        CHECK(d.z > p.z + 0.2f);
        CHECK(std::fabs(d.y - c.y) < 0.001f);
    }

    // Looking up (at the roof), it drops further below the view, so it stays
    // under the eye instead of rising into what is looked at.
    {
        Player p;
        p.pitch = 1.0f;
        CHECK(carryPosition(p).y < L::kEyeHeight);
    }

    // Looking straight down, the angle it rides at stops short of vertical:
    // two steep pitches put it in the same place, still above the floor.
    {
        Player a, b;
        a.pitch = -1.4f;
        b.pitch = -1.5f;
        const Vec3 ca = carryPosition(a), cb = carryPosition(b);
        CHECK(std::fabs(ca.y - cb.y) < 0.0001f && std::fabs(ca.z - cb.z) < 0.0001f);
        CHECK(ca.y >= L::kFloorY + 0.08f);
        CHECK(ca.z < 0);  // still a little ahead
    }

    TEST_MAIN_END();
}
