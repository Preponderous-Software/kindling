#include "sim/target.h"

#include "sim/world.h"

namespace kindling {

namespace {

bool validIndex(int32_t i, size_t n) { return i >= 0 && static_cast<size_t>(i) < n; }

}  // namespace

Description describe(const World& w, const Aim& aim) {
    switch (aim.kind) {
        case TargetKind::None: return {};
        case TargetKind::Floor: return {"Floorboards", "Thick with dust and dead leaves. Sweep them."};
        case TargetKind::Wall: return {"Log wall", "Grimy. Scrub it."};
        case TargetKind::HearthFace: return {"Chimney stones", "Black with old soot. Scrub them."};
        case TargetKind::Firebox: return {"Hearth", "Cold, full of ash."};
        case TargetKind::Debris: {
            if (!validIndex(aim.index, w.debris.size()) || w.debris[aim.index].hauled) return {};
            switch (w.debris[aim.index].kind) {
                case DebrisKind::Board: return {"Broken board", "Haul it out of the cabin."};
                case DebrisKind::Shingle: return {"Fallen shingle", "Haul it out of the cabin."};
                case DebrisKind::Rafter: return {"Fallen rafter", "Too long to carry. Saw it into pieces."};
            }
            return {};
        }
        case TargetKind::RottenPlank: {
            if (!validIndex(aim.index, w.planks.size())) return {};
            switch (w.planks[aim.index]) {
                case PlankState::Rotten: return {"Rotten floorboard", "Pry it up with the pry bar."};
                case PlankState::Removed: return {"Gap in the floor", "Lay a new plank here."};
                case PlankState::New: return {"New floorboard", ""};
            }
            return {};
        }
        case TargetKind::WindowBoard:
            if (!validIndex(aim.index, w.windowBoards.size()) || !w.windowBoards[aim.index]) return {"Window", ""};
            return {"Board nailed over the window", "Pry it off to let the light in."};
        case TargetKind::RoofHole:
            if (w.roofPatches >= 4) return {"Patched roof", ""};
            return {"Hole in the roof", "Patch it with shingles."};
        case TargetKind::Snow:
            if (w.snow <= 0) return {};
            return {"Snow drift", "Shovel it out before it melts into the floor."};
        case TargetKind::Chair:
            return w.chair == ChairState::Broken ? Description{"Broken rocking chair", "Mend it with the hammer."}
                                                 : Description{"Rocking chair", ""};
        case TargetKind::Table:
            return w.table == TableState::Toppled ? Description{"Overturned table", "Set it back on its legs."}
                                                  : Description{"Table", ""};
        case TargetKind::Ceiling: return {"Ceiling boards", ""};
        case TargetKind::Ground: return {};
        case TargetKind::Wagon: return {"Your wagon", "Tools, planks, shingles and furnishings."};
        case TargetKind::ChoppingBlock: return {"Chopping block", "Split log rounds here."};
        case TargetKind::LogPile: return {"Log rounds", "Firewood, still to be split."};
        case TargetKind::HayBale: return {"Hay bale", ""};
        case TargetKind::Stall: return {"Stall", ""};
    }
    return {};
}

uint32_t surveyBit(const World& w, const Aim& aim) {
    switch (aim.kind) {
        case TargetKind::Floor:
        case TargetKind::Wall:
        case TargetKind::HearthFace: return kSurveyGrime;
        case TargetKind::Debris:
            return validIndex(aim.index, w.debris.size()) && !w.debris[aim.index].hauled ? kSurveyDebris : 0;
        case TargetKind::RottenPlank:
            return validIndex(aim.index, w.planks.size()) && w.planks[aim.index] == PlankState::Rotten
                       ? kSurveyRottenPlank
                       : 0;
        case TargetKind::RoofHole: return w.roofPatches < 4 ? kSurveyRoofHole : 0;
        case TargetKind::WindowBoard:
            return validIndex(aim.index, w.windowBoards.size()) && w.windowBoards[aim.index] ? kSurveyBoardedWindow
                                                                                                : 0;
        default: return 0;
    }
}

}  // namespace kindling
