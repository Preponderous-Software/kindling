// M3 Repair and furnish: the pry bar lifts rotten boards and window boards,
// new planks and shingles from the wagon are worked in, the chair is mended,
// the table set right; furnishings go only where they belong, can be moved
// and put back, and Ada's brief is checked against the room.
#include "check.h"
#include "sim/brief.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/rules.h"

using namespace kindling;
namespace L = kindling::layout;

namespace {

Aim aimAt(TargetKind kind, int32_t index, Vec3 point = {}, float distance = 1.0f) {
    Aim a;
    a.kind = kind;
    a.index = index;
    a.point = point;
    a.distance = distance;
    return a;
}

void hold(World& w, Tool tool, const Aim& aim, float seconds, TickEvents* ev = nullptr) {
    Input in;
    in.selectTool = static_cast<int32_t>(tool);
    in.use = true;
    in.aim = aim;
    in.yaw = w.player.yaw;
    const int n = static_cast<int>(seconds * kTickRate + 0.5f);
    for (int i = 0; i < n; ++i) tick(w, in, ev);
}

void press(World& w, const Aim& aim, TickEvents* ev = nullptr) {
    Input in;
    in.interact = true;
    in.aim = aim;
    in.yaw = w.player.yaw;
    tick(w, in, ev);
}

int32_t firstItem(const World& w, ItemKind k) {
    for (size_t i = 0; i < w.items.size(); ++i)
        if (w.items[i].kind == k && w.items[i].place == ItemPlace::Wagon) return static_cast<int32_t>(i);
    return -1;
}

void take(World& w, ItemKind k) { press(w, aimAt(TargetKind::Item, firstItem(w, k))); }

size_t liveDebris(const World& w) {
    size_t n = 0;
    for (const Debris& d : w.debris) n += d.hauled ? 0 : 1;
    return n;
}

}  // namespace

int main() {
    // ---- floorboards: pry up (leaving old wood to haul), then lay a new plank
    {
        World w = newWorld(5);
        const size_t before = liveDebris(w);
        hold(w, Tool::PryBar, aimAt(TargetKind::RottenPlank, 0), 1.0f);
        CHECK(w.planks[0] == PlankState::Rotten);  // not long enough yet
        hold(w, Tool::PryBar, aimAt(TargetKind::RottenPlank, 0), 2.5f);
        CHECK(w.planks[0] == PlankState::Removed);
        CHECK(liveDebris(w) == before + 1);

        // A plank cannot go on a rotten board, only into a gap.
        take(w, ItemKind::Plank);
        CHECK(w.carry == Carry::Item);
        hold(w, Tool::None, aimAt(TargetKind::RottenPlank, 1), 3.0f);
        CHECK(w.planks[1] == PlankState::Rotten);
        CHECK(w.carry == Carry::Item);
        hold(w, Tool::None, aimAt(TargetKind::RottenPlank, 0), 2.1f);
        CHECK(w.planks[0] == PlankState::New);
        CHECK(w.carry == Carry::Nothing);
        int used = 0;
        for (const Item& it : w.items) used += it.place == ItemPlace::Used ? 1 : 0;
        CHECK(used == 1);
    }

    // ---- the roof: a bundle of shingles patches the section aimed at
    {
        World w = newWorld(5);
        take(w, ItemKind::Shingles);
        hold(w, Tool::None, aimAt(TargetKind::RoofHole, 2), 2.6f);
        CHECK(w.roofPatched == (1u << 2));
        CHECK(w.carry == Carry::Nothing);
        // Patching a patched section does nothing (and keeps the bundle).
        take(w, ItemKind::Shingles);
        hold(w, Tool::None, aimAt(TargetKind::RoofHole, 2), 3.0f);
        CHECK(w.carry == Carry::Item);
    }

    // ---- the window, the chair and the table
    {
        World w = newWorld(5);
        for (int b = 0; b < 4; ++b) hold(w, Tool::PryBar, aimAt(TargetKind::WindowBoard, b), 2.1f);
        CHECK(w.windowBoards == (std::array<int32_t, 4>{0, 0, 0, 0}));
        hold(w, Tool::Broom, aimAt(TargetKind::Chair, -1), 4.0f);
        CHECK(w.chair == ChairState::Broken);  // a broom mends nothing
        hold(w, Tool::Hammer, aimAt(TargetKind::Chair, -1), 3.1f);
        CHECK(w.chair == ChairState::Mended);
        press(w, aimAt(TargetKind::Table, -1));
        CHECK(w.table == TableState::Upright);
    }

    // ---- furnishings go where they belong, and nowhere else
    {
        World w = newWorld(5);
        w.player.x = 0;
        w.player.z = 0;
        take(w, ItemKind::Rug);
        TickEvents ev;
        press(w, aimAt(TargetKind::Wall, 0, {0, 1.5f, L::kInnerZ0}), &ev);  // a rug on the wall: no
        CHECK(w.carry == Carry::Item);
        CHECK(!ev.messages.empty());
        press(w, aimAt(TargetKind::Floor, -1, {-0.8f, L::kFloorY, -1.0f}));
        CHECK(w.carry == Carry::Nothing);
        const int32_t rug = firstItem(newWorld(5), ItemKind::Rug);
        CHECK(w.items[static_cast<size_t>(rug)].place == ItemPlace::Placed);

        // The candle goes on the mantel, not the floor.
        take(w, ItemKind::Candle);
        press(w, aimAt(TargetKind::Floor, -1, {0, L::kFloorY, 0}));
        CHECK(w.carry == Carry::Item);
        press(w, aimAt(TargetKind::HearthFace, -1, {0.5f, L::kMantelY + 0.1f, L::kBreastFront}));
        CHECK(w.carry == Carry::Nothing);

        // The quilt needs a mended chair; the curtains an open window.
        take(w, ItemKind::Quilt);
        press(w, aimAt(TargetKind::Chair, -1));
        CHECK(w.carry == Carry::Item);
        press(w, aimAt(TargetKind::Wagon, -1));  // put it back
        CHECK(w.carry == Carry::Nothing);
        CHECK(firstItem(w, ItemKind::Quilt) >= 0);

        // A placed thing can be picked up again and moved; rotating turns it.
        press(w, aimAt(TargetKind::Item, rug));
        CHECK(w.carry == Carry::Item);
        Input turn;
        turn.rotate = 2;
        tick(w, turn);
        press(w, aimAt(TargetKind::Floor, -1, {-1.0f, L::kFloorY, -0.5f}));
        CHECK(w.items[static_cast<size_t>(rug)].place == ItemPlace::Placed);
        CHECK(w.items[static_cast<size_t>(rug)].yaw > 0.5f);
    }

    // ---- Ada's brief, point by point
    {
        World w = newWorld(5);
        CHECK(evaluateBrief(w).metCount() == 0);
        for (int b = 0; b < 4; ++b) w.windowBoards[b] = 0;
        w.table = TableState::Upright;
        w.chair = ChairState::Mended;
        CHECK(evaluateBrief(w).points[0].met);
        auto place = [&](ItemKind k, Vec3 p) {
            const int32_t i = firstItem(w, k);
            w.items[static_cast<size_t>(i)].place = ItemPlace::Placed;
            w.items[static_cast<size_t>(i)].pos = p;
        };
        place(ItemKind::Candle, {0.5f, L::kMantelY, -2.3f});
        place(ItemKind::Curtains, {L::kInnerX0, 2.3f, -0.3f});
        place(ItemKind::Quilt, {1.65f, 0.9f, -0.9f});
        place(ItemKind::Sampler, {0, 1.8f, L::kInnerZ0});
        place(ItemKind::Rug, {3.0f, L::kFloorY, 3.0f});  // by the door, not the hearth
        CHECK(evaluateBrief(w).metCount() == 4);
        CHECK(!evaluateBrief(w).met());
        w.items[static_cast<size_t>(firstItem(newWorld(5), ItemKind::Rug))].pos = {-0.8f, L::kFloorY, -1.0f};
        CHECK(evaluateBrief(w).met());
    }

    // ---- steps: Repair needs everything sound and the old wood out; then Furnish, then Kindle
    {
        World w = newWorld(5);
        w.surveyed = kSurveyAll;
        w.cleared = true;
        CHECK(progress(w).step == Step::Repair);
        w.planks.fill(PlankState::New);
        w.roofPatched = kRoofAllPatched;
        w.windowBoards.fill(0);
        w.chair = ChairState::Mended;
        w.table = TableState::Upright;
        tick(w, Input{});
        CHECK(progress(w).step == Step::Repair);  // the debris is still inside
        for (Debris& d : w.debris) d.hauled = true;
        tick(w, Input{});
        CHECK(w.repaired);
        CHECK(progress(w).step == Step::Furnish);
        int placedCount = 0;
        for (Item& it : w.items)
            if (!isMaterial(it.kind) && placedCount < kFurnishGoal) {
                it.place = ItemPlace::Placed;
                ++placedCount;
            }
        CHECK(progress(w).step == Step::Kindle);
    }

    TEST_MAIN_END();
}
