// What the player is looking at.
//
// The app finds it with Strata's mesh picking (and a few boxes for things with
// no mesh, such as the roof hole) and passes it to the rules in Input::aim;
// tests and the autopilot build an Aim directly. Either way the rules decide
// what an action does from the Aim, never from the engine.
#pragma once

#include <cstdint>
#include <string>

#include "sim/math.h"

namespace kindling {

struct World;

enum class TargetKind : int32_t {
    None = 0,
    Floor,        // index: -1
    Wall,         // index: 0 back, 1 left, 2 right, 3 front
    HearthFace,   // the chimney breast's stones
    Firebox,
    Debris,       // index: into World::debris
    RottenPlank,  // index: into World::planks / layout::kRottenSpans
    WindowBoard,  // index: into World::windowBoards
    RoofHole,     // index: the section (0..3) under the crosshair
    Snow,
    Chair,
    Table,
    Ceiling,
    Ground,       // the yard
    Wagon,
    ChoppingBlock,
    LogPile,
    HayBale,
    Stall,
    Item,         // index: into World::items
};

struct Aim {
    TargetKind kind = TargetKind::None;
    int32_t index = -1;
    Vec3 point;          // where the crosshair ray met it, world space
    float distance = 0;  // from the eye
};

// A short name for the target ("Rotten floorboard"), and what can be done with
// it, or why it matters ("Pry it up with the pry bar"). Empty when there is
// nothing to say.
struct Description {
    std::string name;
    std::string hint;
};
Description describe(const World& w, const Aim& aim);

// The survey bit (one of the kSurvey* bits) a target counts toward, or 0.
uint32_t surveyBit(const World& w, const Aim& aim);

}  // namespace kindling
