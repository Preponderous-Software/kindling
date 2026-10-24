#include "sim/progress.h"

namespace kindling {

const char* stepName(Step s) {
    switch (s) {
        case Step::Survey: return "Survey";
        case Step::Clear: return "Clear";
        case Step::Repair: return "Repair";
        case Step::Furnish: return "Furnish";
        case Step::Kindle: return "Kindle";
        case Step::Welcome: return "Welcome";
        case Step::Done: return "Done";
    }
    return "";
}

Progress progress(const World& w) {
    Progress p;
    p.floorClean = cleanFraction(w.grime, Surface::Floor);
    p.wallsClean = cleanFraction(w.grime, {Surface::BackWall, Surface::LeftWall, Surface::RightWall, Surface::FrontWall});
    p.hearthClean = cleanFraction(w.grime, Surface::HearthFace);
    for (const Debris& d : w.debris) p.debrisLeft += d.hauled ? 0 : 1;
    p.snow = w.snow;
    p.surveyed = (w.surveyed & kSurveyAll) == kSurveyAll;
    p.cleared = p.floorClean >= kFloorCleanGoal && p.wallsClean >= kWallCleanGoal &&
                p.hearthClean >= kWallCleanGoal && p.debrisLeft == 0 && p.snow <= 0;

    for (PlankState s : w.planks) p.planksLeft += s == PlankState::New ? 0 : 1;
    for (int k = 0; k < 4; ++k) p.roofLeft += ((w.roofPatched >> k) & 1u) ? 0 : 1;
    for (int32_t b : w.windowBoards) p.boardsLeft += b ? 1 : 0;
    p.chairMended = w.chair == ChairState::Mended;
    p.tableUpright = w.table == TableState::Upright;
    p.repairedNow = p.planksLeft == 0 && p.roofLeft == 0 && p.boardsLeft == 0 && p.chairMended && p.tableUpright &&
                    p.debrisLeft == 0;
    for (const Item& it : w.items) p.furnished += !isMaterial(it.kind) && it.place == ItemPlace::Placed ? 1 : 0;

    p.ashOut = w.hearth.ash <= 0;
    p.kindlingLaid = w.hearth.kindlingLaid || w.hearth.lit;
    p.logsLaid = w.hearth.logsLaid > 0 || w.hearth.lit;
    // A log taken from the stack for the fire still counts: it is burning, or
    // laid on the andirons.
    // A split log in hand once the kindling is laid is on its way from the
    // stack to the fire: it still counts.
    const bool logInHand = w.carry == Carry::Firewood && w.carryIndex == static_cast<int32_t>(Wood::Split) &&
                           w.hearth.kindlingLaid;
    p.woodCount = w.wood.woodpile + (w.hearth.lit ? (w.hearth.fuelMinutes > 0 ? 1 : 0) : w.hearth.logsLaid) +
                  (logInHand ? 1 : 0);
    p.woodStacked = p.woodCount >= kWoodpileGoal;
    p.burning = w.hearth.lit && w.hearth.fire >= kBurningFire;

    // Steps are reached in order: later work done early counts when its step comes.
    if (!p.surveyed) p.step = Step::Survey;
    else if (!w.cleared) p.step = Step::Clear;
    else if (!w.repaired) p.step = Step::Repair;
    else if (p.furnished < kFurnishGoal) p.step = Step::Furnish;
    else if (!w.kindled) p.step = Step::Kindle;
    else if (!w.settler.done) p.step = Step::Welcome;
    else p.step = Step::Done;
    return p;
}

}  // namespace kindling
