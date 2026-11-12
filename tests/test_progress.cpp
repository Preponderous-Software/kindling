// Progress: the step the player is on and the numbers behind each step's
// checklist, read straight off a World. Steps come in order, and a step once
// finished stays finished (World::cleared, repaired, kindled latch it).
#include <cstring>
#include <string>

#include "check.h"
#include "sim/progress.h"

using namespace kindling;

namespace {

// Makes `share` of a surface filthy and the rest clean.
void soil(World& w, Surface s, float share) {
    std::vector<uint8_t>& t = w.grime.texels[static_cast<size_t>(s)];
    const size_t dirty = static_cast<size_t>(share * static_cast<float>(t.size()) + 0.5f);
    for (size_t i = 0; i < t.size(); ++i) t[i] = i < dirty ? 255 : 0;
}

// The cabin with every Clear goal met: no grime, no debris inside, no snow.
World clearedRoom() {
    World w = newWorld(1881);
    for (int s = 0; s < kSurfaceCount; ++s) soil(w, static_cast<Surface>(s), 0);
    for (Debris& d : w.debris) d.hauled = true;
    w.snow = 0;
    return w;
}

// Places the first `n` furnishings; returns how many were placed.
int furnish(World& w, int n) {
    int placed = 0;
    for (Item& it : w.items)
        if (!isMaterial(it.kind) && placed < n) {
            it.place = ItemPlace::Placed;
            ++placed;
        }
    return placed;
}

}  // namespace

int main() {
    // ---- the steps' names, as the HUD shows them
    {
        const char* names[] = {"Survey", "Clear", "Repair", "Furnish", "Kindle", "Welcome", "Done"};
        for (int s = 0; s <= static_cast<int>(Step::Done); ++s)
            CHECK(std::strcmp(stepName(static_cast<Step>(s)), names[s]) == 0);
        CHECK(std::string(stepName(static_cast<Step>(99))).empty());
    }

    // ---- a new game starts at the Survey, with everything still to do
    {
        const World w = newWorld(1881);
        const Progress p = progress(w);
        CHECK(p.step == Step::Survey);
        CHECK(!p.surveyed && !p.cleared && !p.repairedNow);
        CHECK(p.debrisLeft == static_cast<int32_t>(w.debris.size()) && p.debrisLeft > 0);
        CHECK(p.planksLeft == 3 && p.roofLeft == 4 && p.boardsLeft == 4);
        CHECK(!p.chairMended && !p.tableUpright);
        CHECK(p.furnished == 0);
        CHECK(!p.ashOut && !p.kindlingLaid && !p.logsLaid && !p.burning);
        CHECK(p.woodCount == 2 && !p.woodStacked);  // the two old logs on the woodpile
    }

    // ---- steps are reached in order; later work done early waits its turn
    {
        World w = newWorld(1881);
        w.cleared = w.repaired = w.kindled = true;
        w.settler.done = true;
        CHECK(furnish(w, kFurnishGoal) == kFurnishGoal);
        // Every later step is finished, but one kind of damage is still unseen.
        w.surveyed = kSurveyAll & ~kSurveyRoofHole;
        CHECK(progress(w).step == Step::Survey);
        w.surveyed = kSurveyAll;
        CHECK(progress(w).step == Step::Done);

        World s = w;
        s.cleared = false;
        CHECK(progress(s).step == Step::Clear);
        s = w;
        s.repaired = false;
        CHECK(progress(s).step == Step::Repair);
        s = w;
        for (Item& it : s.items)
            if (it.place == ItemPlace::Placed) {
                it.place = ItemPlace::Wagon;  // one short of the goal
                break;
            }
        CHECK(progress(s).furnished == kFurnishGoal - 1);
        CHECK(progress(s).step == Step::Furnish);
        s = w;
        s.kindled = false;
        CHECK(progress(s).step == Step::Kindle);
        s = w;
        s.settler.done = false;
        CHECK(progress(s).step == Step::Welcome);
    }

    // ---- a finished step stays finished: the latch, not the room, decides
    {
        World w = newWorld(1881);
        w.surveyed = kSurveyAll;
        w.cleared = true;  // cleared once, then a new mess (the ruin's own grime)
        const Progress p = progress(w);
        CHECK(!p.cleared);
        CHECK(p.step == Step::Repair);
        // And the room meeting the goals does not finish the step by itself.
        World c = clearedRoom();
        c.surveyed = kSurveyAll;
        CHECK(progress(c).cleared);
        CHECK(progress(c).step == Step::Clear);
    }

    // ---- what "clear" means: each goal on its own holds the step back
    {
        CHECK(progress(clearedRoom()).cleared);
        CHECK(progress(clearedRoom()).floorClean == 1.0f);

        World w = clearedRoom();
        soil(w, Surface::Floor, 0.2f);  // 80% clean: short of the floor's 85%
        CHECK(!progress(w).cleared);
        soil(w, Surface::Floor, 0.1f);  // 90% clean
        CHECK(progress(w).cleared);

        w = clearedRoom();
        for (Surface s : {Surface::BackWall, Surface::LeftWall, Surface::RightWall, Surface::FrontWall})
            soil(w, s, 0.3f);  // the walls 70% clean: short of 75%
        CHECK(!progress(w).cleared);
        for (Surface s : {Surface::BackWall, Surface::LeftWall, Surface::RightWall, Surface::FrontWall})
            soil(w, s, 0.2f);  // 80%: enough for walls, though not for a floor
        CHECK(progress(w).cleared);

        // The chimney stones are held to the walls' goal, not the floor's.
        w = clearedRoom();
        soil(w, Surface::HearthFace, 0.2f);
        CHECK(progress(w).cleared);
        soil(w, Surface::HearthFace, 0.3f);
        CHECK(!progress(w).cleared);

        w = clearedRoom();
        w.debris[0].hauled = false;
        CHECK(progress(w).debrisLeft == 1);
        CHECK(!progress(w).cleared);

        w = clearedRoom();
        w.snow = 0.01f;
        CHECK(!progress(w).cleared);
    }

    // ---- the Repair checklist
    {
        World w = newWorld(1881);
        w.planks = {PlankState::Rotten, PlankState::Removed, PlankState::New};
        CHECK(progress(w).planksLeft == 2);  // a lifted board is still a gap in the floor
        w.roofPatched = 0b0101;
        CHECK(progress(w).roofLeft == 2);
        w.windowBoards = {0, 1, 0, 0};
        CHECK(progress(w).boardsLeft == 1);

        // Everything mended, but the old wood still lies inside.
        w.planks.fill(PlankState::New);
        w.roofPatched = kRoofAllPatched;
        w.windowBoards.fill(0);
        w.chair = ChairState::Mended;
        w.table = TableState::Upright;
        Progress p = progress(w);
        CHECK(p.planksLeft == 0 && p.roofLeft == 0 && p.boardsLeft == 0 && p.chairMended && p.tableUpright);
        CHECK(!p.repairedNow);
        for (Debris& d : w.debris) d.hauled = true;
        CHECK(progress(w).repairedNow);
        w.chair = ChairState::Broken;
        CHECK(!progress(w).repairedNow);
    }

    // ---- only furnishings set in place count as furnished
    {
        World w = newWorld(1881);
        for (Item& it : w.items)
            if (isMaterial(it.kind)) it.place = ItemPlace::Placed;
        CHECK(progress(w).furnished == 0);
        for (Item& it : w.items)
            if (!isMaterial(it.kind)) {
                it.place = ItemPlace::Carried;
                break;
            }
        CHECK(progress(w).furnished == 0);
        CHECK(furnish(w, 2) == 2);
        CHECK(progress(w).furnished == 2);
    }

    // ---- the Kindle checklist
    {
        World w = newWorld(1881);
        w.hearth.ash = 0.01f;
        CHECK(!progress(w).ashOut);
        w.hearth.ash = 0;
        CHECK(progress(w).ashOut);

        // Unlit: the logs on the andirons count with the woodpile.
        w.wood.woodpile = 2;
        w.hearth.logsLaid = 3;
        CHECK(progress(w).woodCount == 5);
        CHECK(progress(w).woodStacked);
        CHECK(progress(w).logsLaid && !progress(w).kindlingLaid);

        // Lit: the laid logs have become the fire, which counts as one log
        // while it has fuel, and none once it has burned down.
        w.hearth.lit = true;
        w.hearth.fuelMinutes = 30;
        CHECK(progress(w).woodCount == 3);
        w.hearth.fuelMinutes = 0;
        CHECK(progress(w).woodCount == 2);
        // A lit fire stands for the kindling and logs laid under it.
        w.hearth.logsLaid = 0;
        w.hearth.kindlingLaid = false;
        CHECK(progress(w).kindlingLaid && progress(w).logsLaid);

        // Burning means lit and grown past a flicker.
        w.hearth.fire = kBurningFire - 0.01f;
        CHECK(!progress(w).burning);
        w.hearth.fire = kBurningFire;
        CHECK(progress(w).burning);
        w.hearth.lit = false;
        CHECK(!progress(w).burning);
    }

    // ---- a split log in hand counts only once the kindling is laid
    {
        World w = newWorld(1881);
        w.wood.woodpile = 1;
        w.carry = Carry::Firewood;
        w.carryIndex = static_cast<int32_t>(Wood::Split);
        CHECK(progress(w).woodCount == 1);  // fetched before there is a fire to lay
        w.hearth.kindlingLaid = true;
        CHECK(progress(w).woodCount == 2);
        w.carryIndex = static_cast<int32_t>(Wood::Round);  // a round is not firewood yet
        CHECK(progress(w).woodCount == 1);
        w.carryIndex = static_cast<int32_t>(Wood::Kindling);
        CHECK(progress(w).woodCount == 1);
    }

    TEST_MAIN_END();
}
