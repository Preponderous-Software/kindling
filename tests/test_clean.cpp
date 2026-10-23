// M2 Clear: sweeping and scrubbing clean exactly the patch aimed at, debris is
// carried out of the door (or tossed out through it), the work meter fills, the rafter is sawn before it can be carried, the
// snow is shovelled out, and the step moves on once everything is clear.
#include <cmath>
#include <cstdio>
#include <string>

#include "check.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/rules.h"

using namespace kindling;
namespace L = kindling::layout;

namespace {

Aim aimAt(TargetKind kind, int32_t index, Vec3 point, float distance = 1.0f) {
    Aim a;
    a.kind = kind;
    a.index = index;
    a.point = point;
    a.distance = distance;
    return a;
}

// Hold the primary action on a target for n ticks without moving.
void hold(World& w, Tool tool, const Aim& aim, int n, TickEvents* ev = nullptr) {
    Input in;
    in.selectTool = static_cast<int32_t>(tool);
    in.use = true;
    in.aim = aim;
    in.yaw = w.player.yaw;
    for (int i = 0; i < n; ++i) tick(w, in, ev);
}

void press(World& w, const Aim& aim) {
    Input in;
    in.interact = true;
    in.aim = aim;
    in.yaw = w.player.yaw;
    tick(w, in);
}

// Teleport for the test: the rules only care where the player ends up.
void standAt(World& w, float x, float z, float yaw = 0) {
    w.player.x = x;
    w.player.z = z;
    w.player.yaw = yaw;
}

}  // namespace

int main() {
    // ---- the broom cleans a disc of floor round the aim point, and only there
    {
        World w = newWorld(3);
        standAt(w, 0, 2);
        const Vec3 spot{0.5f, L::kFloorY, 1.0f}, far{-2.5f, L::kFloorY, 3.0f};
        int i, j, fi, fj;
        CHECK(texelAt(Surface::Floor, spot, i, j));
        CHECK(texelAt(Surface::Floor, far, fi, fj));
        const uint8_t farBefore = w.grime.at(Surface::Floor, fi, fj);
        CHECK(w.grime.at(Surface::Floor, i, j) > kCleanThreshold);
        TickEvents ev;
        hold(w, Tool::Broom, aimAt(TargetKind::Floor, -1, spot), kTickRate, &ev);
        CHECK(w.grime.at(Surface::Floor, i, j) == 0);
        CHECK(w.grime.at(Surface::Floor, fi, fj) == farBefore);
        CHECK(ev.grime[static_cast<size_t>(Surface::Floor)].w > 0);  // the app is told what to re-upload
        CHECK(ev.grime[static_cast<size_t>(Surface::BackWall)].w == 0);

        // The wrong tool does nothing: a scrub brush on the floor, a broom on a wall.
        World x = newWorld(3);
        standAt(x, 0, 2);
        hold(x, Tool::ScrubBrush, aimAt(TargetKind::Floor, -1, spot), kTickRate);
        CHECK(x.grime.at(Surface::Floor, i, j) == newWorld(3).grime.at(Surface::Floor, i, j));
        const Vec3 wallSpot{0.0f, 1.2f, L::kInnerZ0};
        int wi, wj;
        CHECK(texelAt(Surface::BackWall, wallSpot, wi, wj));
        const uint8_t wallBefore = x.grime.at(Surface::BackWall, wi, wj);
        hold(x, Tool::Broom, aimAt(TargetKind::Wall, 0, wallSpot), kTickRate);
        CHECK(x.grime.at(Surface::BackWall, wi, wj) == wallBefore);
        hold(x, Tool::ScrubBrush, aimAt(TargetKind::Wall, 0, wallSpot), kTickRate);
        CHECK(x.grime.at(Surface::BackWall, wi, wj) == 0);

        // Out of reach does nothing either.
        World y = newWorld(3);
        hold(y, Tool::Broom, aimAt(TargetKind::Floor, -1, spot, L::kReach + 0.5f), kTickRate);
        CHECK(y.grime.at(Surface::Floor, i, j) == newWorld(3).grime.at(Surface::Floor, i, j));
    }

    // ---- debris: carried with interact, hauled out by putting it down outdoors
    {
        World w = newWorld(3);
        standAt(w, 0, 1);
        const int32_t board = 0;
        press(w, aimAt(TargetKind::Debris, board, w.debris[board].pos));
        CHECK(w.carry == Carry::Debris && w.carryIndex == board);
        // While carried it rides with the player.
        Input walk;
        walk.forward = 1;
        walk.yaw = 3.14159265f;  // toward the door
        for (int i = 0; i < 10; ++i) tick(w, walk);
        CHECK(w.debris[board].pos.z > 1.5f);
        // Put down indoors: it stays inside, on the floor.
        press(w, Aim{});
        CHECK(w.carry == Carry::Nothing);
        CHECK(!w.debris[board].hauled);
        CHECK(w.debris[board].pos.y < 0.2f);
        // Carried outside and put down: hauled out.
        press(w, aimAt(TargetKind::Debris, board, w.debris[board].pos));
        standAt(w, 2.5f, 6.0f);
        TickEvents ev;
        Input put;
        put.interact = true;
        tick(w, put, &ev);
        CHECK(w.debris[board].hauled);
        CHECK(w.debrisHauled == 1);
        CHECK(!ev.messages.empty());
    }

    // ---- debris put down indoors by the door is tossed out through it; elsewhere indoors it stays
    {
        World w = newWorld(11);
        const int32_t n = static_cast<int32_t>(w.debris.size());
        standAt(w, w.debris[0].pos.x, w.debris[0].pos.z + 0.8f);
        press(w, aimAt(TargetKind::Debris, 0, w.debris[0].pos));
        CHECK(w.carry == Carry::Debris);
        standAt(w, 0, 0);
        CHECK(!byTheDoor(w));
        press(w, Aim{});
        CHECK(!w.debris[0].hauled && w.carry == Carry::Nothing);
        press(w, aimAt(TargetKind::Debris, 0, w.debris[0].pos));
        standAt(w, (L::kDoorX0 + L::kDoorX1) / 2, L::kInnerZ1 - 0.9f);
        CHECK(byTheDoor(w));
        TickEvents ev;
        Input in;
        in.interact = true;
        tick(w, in, &ev);
        CHECK(w.debris[0].hauled && w.debrisHauled == 1 && w.carry == Carry::Nothing);
        bool said = false;
        for (const std::string& m : ev.messages) said = said || m.find("Tossed out the door") == 0;
        CHECK(said);
        CHECK(progress(w).debrisLeft == n - 1);
    }

    // ---- scrubbing one wall moves the walls figure at once (#17), and the meter shows the patch
    {
        World w = newWorld(5);
        standAt(w, 0, 2);
        const Vec3 spot{0.5f, 1.2f, L::kInnerZ0};
        const Aim wall = aimAt(TargetKind::Wall, 0, spot);
        const float before = progress(w).wallsClean;
        CHECK(workMeter(w, wall) < 0);  // no brush in hand
        hold(w, Tool::ScrubBrush, wall, 1);
        const float start = workMeter(w, wall);
        CHECK(start >= 0 && start < 1);
        hold(w, Tool::ScrubBrush, wall, 10 * kTickRate);
        CHECK(workMeter(w, wall) >= 0.99f);
        CHECK(progress(w).wallsClean > before);
        CHECK(workMeter(w, aimAt(TargetKind::Wall, 0, spot, L::kReach + 0.5f)) < 0);  // out of reach: no meter
    }

    // ---- the meter for a timed job fills with the time held
    {
        World w = newWorld(5);
        const Aim board = aimAt(TargetKind::WindowBoard, 1, {L::kInnerX0, 1.4f, 0});
        hold(w, Tool::PryBar, board, kTickRate);
        CHECK(std::fabs(workMeter(w, board) - 0.5f) < 0.05f);
    }

    // ---- the rafter must be sawn before it can be carried
    {
        World w = newWorld(3);
        standAt(w, -1.0f, 1.0f);
        const int32_t rafter = static_cast<int32_t>(w.debris.size()) - 1;
        CHECK(w.debris[static_cast<size_t>(rafter)].kind == DebrisKind::Rafter);
        TickEvents ev;
        Input in;
        in.interact = true;
        in.aim = aimAt(TargetKind::Debris, rafter, w.debris[static_cast<size_t>(rafter)].pos);
        tick(w, in, &ev);
        CHECK(w.carry == Carry::Nothing);
        CHECK(!ev.messages.empty());
        // Sawing stops short if you let go, and finishes after four seconds.
        hold(w, Tool::Saw, in.aim, 2 * kTickRate);
        CHECK(!w.debris[static_cast<size_t>(rafter)].hauled);
        tick(w, Input{});  // let go
        hold(w, Tool::Saw, in.aim, 3 * kTickRate);
        CHECK(!w.debris[static_cast<size_t>(rafter)].hauled);
        hold(w, Tool::Saw, in.aim, kTickRate + 2);
        CHECK(w.debris[static_cast<size_t>(rafter)].hauled);
        CHECK(w.debris.size() == 18);
        CHECK(w.debris.back().kind == DebrisKind::RafterPiece);
    }

    // ---- the snow drift is shovelled out
    {
        World w = newWorld(3);
        standAt(w, 0.1f, 0.3f);
        const Aim snow = aimAt(TargetKind::Snow, -1, {0.1f, 0.1f, -0.9f});
        hold(w, Tool::Broom, snow, kTickRate);
        CHECK(w.snow == 1.0f);
        hold(w, Tool::Shovel, snow, 3 * kTickRate);
        CHECK(w.snow < 0.6f && w.snow > 0.4f);
        hold(w, Tool::Shovel, snow, 5 * kTickRate);
        CHECK(w.snow == 0.0f);
    }

    // ---- the Clear step ends once everything is clean, hauled and shovelled
    {
        World w = newWorld(3);
        w.surveyed = kSurveyAll;
        CHECK(progress(w).step == Step::Clear);
        for (std::vector<uint8_t>& t : w.grime.texels)
            for (uint8_t& v : t) v = 0;
        for (Debris& d : w.debris) d.hauled = true;
        CHECK(progress(w).step == Step::Clear);  // the snow is still there
        w.snow = 0;
        CHECK(progress(w).cleared);
        tick(w, Input{});  // the tick that notices latches it
        CHECK(w.cleared);
        CHECK(progress(w).step == Step::Repair);
        // A new mess later does not send the player back to Clear.
        w.debris[0].hauled = false;
        tick(w, Input{});
        CHECK(progress(w).step == Step::Repair);
    }

    TEST_MAIN_END();
}
