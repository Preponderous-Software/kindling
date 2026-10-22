#include "sim/progress.h"

#include <algorithm>

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
    p.wallsClean = 1.0f;
    for (Surface s : {Surface::BackWall, Surface::LeftWall, Surface::RightWall, Surface::FrontWall})
        p.wallsClean = std::min(p.wallsClean, cleanFraction(w.grime, s));
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

    // Steps are reached in order: later work done early counts when its step comes.
    if (!p.surveyed) p.step = Step::Survey;
    else if (!w.cleared) p.step = Step::Clear;
    else if (!w.repaired) p.step = Step::Repair;
    else if (p.furnished < kFurnishGoal) p.step = Step::Furnish;
    else p.step = Step::Kindle;
    return p;
}

}  // namespace kindling
