// M4 Kindle: rounds are split on the block into logs and kindling, the ash is
// shovelled out, the fire is laid and lit by holding a match to it (letting go
// too soon puts the match out), it grows and burns its fuel down, the candle
// takes a flame, and the clock runs from morning into the first frost.
#include "check.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/rules.h"

using namespace kindling;

namespace {

Aim aimAt(TargetKind kind, int32_t index = -1) {
    Aim a;
    a.kind = kind;
    a.index = index;
    a.distance = 1.0f;
    return a;
}

void hold(World& w, Tool tool, const Aim& aim, float seconds, TickEvents* ev = nullptr) {
    Input in;
    in.selectTool = static_cast<int32_t>(tool);
    in.use = true;
    in.aim = aim;
    const int n = static_cast<int>(seconds * kTickRate + 0.5f);
    for (int i = 0; i < n; ++i) tick(w, in, ev);
}

void press(World& w, const Aim& aim, TickEvents* ev = nullptr) {
    Input in;
    in.interact = true;
    in.aim = aim;
    tick(w, in, ev);
}

void idle(World& w, float seconds, TickEvents* ev = nullptr) {
    const int n = static_cast<int>(seconds * kTickRate + 0.5f);
    for (int i = 0; i < n; ++i) tick(w, Input{}, ev);
}

}  // namespace

int main() {
    // ---- the firewood chain
    {
        World w = newWorld(9);
        const int32_t rounds = w.wood.rounds;
        press(w, aimAt(TargetKind::LogPile));
        CHECK(w.carry == Carry::Firewood && w.carryIndex == static_cast<int32_t>(Wood::Round));
        CHECK(w.wood.rounds == rounds - 1);
        // A round cannot go in the hearth or on the woodpile.
        press(w, aimAt(TargetKind::Woodpile));
        CHECK(w.carry == Carry::Firewood);
        press(w, aimAt(TargetKind::ChoppingBlock));
        CHECK(w.carry == Carry::Nothing && w.wood.roundOnBlock);
        hold(w, Tool::Axe, aimAt(TargetKind::ChoppingBlock), 1.0f);
        CHECK(w.wood.roundOnBlock);
        hold(w, Tool::Axe, aimAt(TargetKind::ChoppingBlock), 1.6f);
        CHECK(!w.wood.roundOnBlock);
        CHECK(w.wood.splitByBlock == 2 && w.wood.kindlingByBlock == 1);
        // Split logs stack on the woodpile inside.
        const int32_t pile = w.wood.woodpile;
        press(w, aimAt(TargetKind::SplitWood));
        press(w, aimAt(TargetKind::Woodpile));
        CHECK(w.wood.woodpile == pile + 1 && w.carry == Carry::Nothing);
    }

    // ---- laying and lighting the fire
    {
        World w = newWorld(9);
        w.wood.kindlingByBlock = 1;
        TickEvents ev;
        // Kindling will not go in on top of the old ash.
        press(w, aimAt(TargetKind::Kindling));
        press(w, aimAt(TargetKind::Firebox), &ev);
        CHECK(w.carry == Carry::Firewood && !w.hearth.kindlingLaid);
        // Hands full: no shovelling until the kindling is put back.
        hold(w, Tool::Shovel, aimAt(TargetKind::Firebox), 1.0f);
        CHECK(w.hearth.ash == 1.0f);
        press(w, aimAt(TargetKind::Kindling));
        CHECK(w.carry == Carry::Nothing);
        hold(w, Tool::Shovel, aimAt(TargetKind::Firebox), 3.5f);
        CHECK(w.hearth.ash == 0.0f);
        press(w, aimAt(TargetKind::Kindling));
        press(w, aimAt(TargetKind::Firebox));
        CHECK(w.hearth.kindlingLaid && w.carry == Carry::Nothing);
        // A match does nothing until there is a log on the andirons.
        hold(w, Tool::Matches, aimAt(TargetKind::Firebox), 5.0f);
        CHECK(!w.hearth.lit);
        press(w, aimAt(TargetKind::Woodpile));
        press(w, aimAt(TargetKind::Firebox));
        CHECK(w.hearth.logsLaid == 1);
        // Letting go too soon: the match goes out.
        TickEvents out;
        hold(w, Tool::Matches, aimAt(TargetKind::Firebox), 2.0f);
        tick(w, Input{}, &out);
        CHECK(!w.hearth.lit);
        CHECK(w.hearth.matchesStruck == 1);
        CHECK(!out.messages.empty());
        // Held long enough: it catches, and grows.
        hold(w, Tool::Matches, aimAt(TargetKind::Firebox), 4.1f);
        CHECK(w.hearth.lit);
        CHECK(w.hearth.fire < 0.2f);
        idle(w, 30.0f);
        CHECK(w.hearth.fire > 0.9f);
        CHECK(w.hearth.fuelMinutes > 0);
        // It burns its fuel (a log is four game hours: twelve minutes) and dies
        // down to coals.
        idle(w, 760.0f);
        CHECK(!w.hearth.lit);
        CHECK(w.hearth.fire == 0.0f);
    }

    // ---- a log on a burning fire feeds it
    {
        World w = newWorld(9);
        w.hearth.ash = 0;
        w.hearth.lit = true;
        w.hearth.fire = 1.0f;
        w.hearth.fuelMinutes = 10;
        press(w, aimAt(TargetKind::Woodpile));
        press(w, aimAt(TargetKind::Firebox));
        CHECK(w.hearth.fuelMinutes > 240);
    }

    // ---- the candle on the mantel takes a flame
    {
        World w = newWorld(9);
        int32_t candle = -1;
        for (size_t i = 0; i < w.items.size(); ++i)
            if (w.items[i].kind == ItemKind::Candle) candle = static_cast<int32_t>(i);
        hold(w, Tool::Matches, aimAt(TargetKind::Item, candle), 1.5f);
        CHECK(!w.hearth.candleLit);  // not while it is still on the wagon
        w.items[static_cast<size_t>(candle)].place = ItemPlace::Placed;
        hold(w, Tool::Matches, aimAt(TargetKind::Item, candle), 1.1f);
        CHECK(w.hearth.candleLit);
    }

    // ---- the clock: late morning to the first frost
    {
        World w = newWorld(9);
        CHECK(dayNumber(w) == 0);
        CHECK(timeOfDay(w) > 0.37 && timeOfDay(w) < 0.38);  // 9:00
        CHECK(!frosted(w));
        TickEvents ev;
        idle(w, 7.5f * 180.0f + 1.0f, &ev);  // to 16:30, three real minutes a game hour
        CHECK(dusk(w) && !frosted(w));
        bool lantern = false;
        for (const std::string& m : ev.messages) lantern = lantern || m.find("lantern") != std::string::npos;
        CHECK(lantern);
        idle(w, 3.5f * 180.0f, &ev);  // to 20:00
        CHECK(frosted(w));
        bool told = false;
        for (const std::string& m : ev.messages) told = told || m.find("frost") != std::string::npos;
        CHECK(told);
    }

    // ---- once Ada is settled by the fire, she keeps it burning
    {
        World w = newWorld(9);
        w.hearth.ash = 0;
        w.hearth.lit = true;
        w.hearth.fire = 1.0f;
        w.hearth.fuelMinutes = 10;
        w.settler.state = SettlerState::Seated;
        idle(w, 3 * 760.0f);
        CHECK(w.hearth.lit);
        CHECK(w.hearth.fire > 0.9f);
    }

    // ---- the Kindle step ends with a fire burning and wood stacked by the hearth
    {
        World w = newWorld(9);
        w.surveyed = kSurveyAll;
        w.cleared = w.repaired = true;
        int n = 0;
        for (Item& it : w.items)
            if (!isMaterial(it.kind) && n < kFurnishGoal) {
                it.place = ItemPlace::Placed;
                ++n;
            }
        CHECK(progress(w).step == Step::Kindle);
        w.hearth.ash = 0;
        w.hearth.lit = true;
        w.hearth.fire = 1.0f;
        w.hearth.fuelMinutes = 500;
        tick(w, Input{});
        CHECK(progress(w).step == Step::Kindle);  // only two logs stacked
        // A log taken from the stack for the fire still counts toward it.
        w.wood.woodpile = kWoodpileGoal - 1;
        CHECK(progress(w).woodCount == kWoodpileGoal);
        tick(w, Input{});
        CHECK(w.kindled);
        CHECK(progress(w).step == Step::Welcome);
    }

    TEST_MAIN_END();
}
