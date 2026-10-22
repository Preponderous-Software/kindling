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
    // Steps are reached in order: later work done early waits for the earlier steps.
    p.step = !p.surveyed ? Step::Survey : !p.cleared ? Step::Clear : Step::Repair;
    return p;
}

}  // namespace kindling
