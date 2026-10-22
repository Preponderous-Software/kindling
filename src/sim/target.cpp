#include "sim/target.h"

#include "sim/items.h"
#include "sim/world.h"

namespace kindling {

namespace {

bool validIndex(int32_t i, size_t n) { return i >= 0 && static_cast<size_t>(i) < n; }

Surface wallSurfaceOf(int32_t index) {
    switch (index) {
        case 0: return Surface::BackWall;
        case 1: return Surface::LeftWall;
        case 2: return Surface::RightWall;
        default: return Surface::FrontWall;
    }
}

bool dirtyAt(const World& w, Surface s, Vec3 p) {
    int i, j;
    return texelAt(s, p, i, j) && w.grime.at(s, i, j) > kCleanThreshold;
}

}  // namespace

Description describe(const World& w, const Aim& aim) {
    switch (aim.kind) {
        case TargetKind::None: return {};
        case TargetKind::Floor:
            return dirtyAt(w, Surface::Floor, aim.point)
                       ? Description{"Floorboards", "Thick with dust and dead leaves. Sweep them (1)."}
                       : Description{"Floorboards", ""};
        case TargetKind::Wall:
            return dirtyAt(w, wallSurfaceOf(aim.index), aim.point) ? Description{"Log wall", "Grimy. Scrub it (2)."}
                                                                   : Description{"Log wall", ""};
        case TargetKind::HearthFace:
            return dirtyAt(w, Surface::HearthFace, aim.point)
                       ? Description{"Chimney stones", "Black with old soot. Scrub them (2)."}
                       : Description{"Chimney stones", ""};
        case TargetKind::Firebox: return {"Hearth", "Cold, full of ash."};
        case TargetKind::Debris: {
            if (!validIndex(aim.index, w.debris.size()) || w.debris[aim.index].hauled) return {};
            switch (w.debris[aim.index].kind) {
                case DebrisKind::Board: return {"Broken board", "Haul it out of the cabin."};
                case DebrisKind::Shingle: return {"Fallen shingle", "Haul it out of the cabin."};
                case DebrisKind::Rafter: return {"Fallen rafter", "Too long to carry. Saw it into pieces (4)."};
                case DebrisKind::RafterPiece: return {"Length of rafter", "Haul it out of the cabin."};
            }
            return {};
        }
        case TargetKind::RottenPlank: {
            if (!validIndex(aim.index, w.planks.size())) return {};
            switch (w.planks[aim.index]) {
                case PlankState::Rotten: return {"Rotten floorboard", "Pry it up with the pry bar (5)."};
                case PlankState::Removed: return {"Gap in the floor", "Bring a new plank from the wagon and work it in."};
                case PlankState::New: return {"New floorboard", ""};
            }
            return {};
        }
        case TargetKind::WindowBoard:
            if (!validIndex(aim.index, w.windowBoards.size()) || !w.windowBoards[aim.index]) return {"Window", ""};
            return {"Board nailed over the window", "Pry it off to let the light in (5)."};
        case TargetKind::RoofHole:
            if (w.roofPatched == kRoofAllPatched) return {"Patched roof", ""};
            return {"Hole in the roof", "Bring shingles from the wagon and work them in."};
        case TargetKind::Snow:
            if (w.snow <= 0) return {};
            return {"Snow drift", "Shovel it out (3) before it melts into the floor."};
        case TargetKind::Chair:
            return w.chair == ChairState::Broken ? Description{"Broken rocking chair", "Mend it with the hammer (6)."}
                                                 : Description{"Rocking chair", ""};
        case TargetKind::Table:
            return w.table == TableState::Toppled ? Description{"Overturned table", "Set it back on its legs (E)."}
                                                  : Description{"Table", ""};
        case TargetKind::Ceiling: return {"Ceiling boards", ""};
        case TargetKind::Ground: return {};
        case TargetKind::Wagon: return {"Your wagon", "Tools, planks, shingles and furnishings."};
        case TargetKind::ChoppingBlock: return {"Chopping block", "Split log rounds here."};
        case TargetKind::LogPile: return {"Log rounds", "Firewood, still to be split."};
        case TargetKind::HayBale: return {"Hay bale", ""};
        case TargetKind::Stall: return {"Stall", ""};
        case TargetKind::Item: {
            if (!validIndex(aim.index, w.items.size())) return {};
            const Item& it = w.items[static_cast<size_t>(aim.index)];
            if (it.place == ItemPlace::Wagon) return {itemName(it.kind), "Take it (E)."};
            if (it.place == ItemPlace::Placed) return {itemName(it.kind), "Move it (E)."};
            return {};
        }
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
        case TargetKind::RoofHole: return w.roofPatched != kRoofAllPatched ? kSurveyRoofHole : 0;
        case TargetKind::WindowBoard:
            return validIndex(aim.index, w.windowBoards.size()) && w.windowBoards[aim.index] ? kSurveyBoardedWindow
                                                                                                : 0;
        default: return 0;
    }
}

}  // namespace kindling
