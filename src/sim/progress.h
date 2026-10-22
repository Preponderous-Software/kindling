// How far the restoration has come: which of the six steps the player is on,
// and the numbers behind each step's checklist.
#pragma once

#include <cstdint>

#include "sim/world.h"

namespace kindling {

enum class Step : int32_t { Survey = 0, Clear, Repair, Furnish, Kindle, Welcome, Done };
const char* stepName(Step s);

// What "clear" means: these shares of each surface at or below the clean threshold.
constexpr float kFloorCleanGoal = 0.85f;
constexpr float kWallCleanGoal = 0.75f;  // each wall, and the chimney stones

struct Progress {
    Step step = Step::Survey;
    float floorClean = 0;   // 0..1
    float wallsClean = 0;   // the least clean wall, 0..1
    float hearthClean = 0;  // the chimney stones, 0..1
    int32_t debrisLeft = 0;  // pieces still inside the cabin
    float snow = 0;
    bool surveyed = false;
    bool cleared = false;
};

Progress progress(const World& w);

}  // namespace kindling
