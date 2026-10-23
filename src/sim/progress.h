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
constexpr float kWallCleanGoal = 0.75f;  // the four walls together, and the chimney stones

// What "furnished" means for the MVP: this many furnishings placed.
constexpr int kFurnishGoal = 5;

struct Progress {
    Step step = Step::Survey;
    float floorClean = 0;   // 0..1
    float wallsClean = 0;   // the four walls together, 0..1
    float hearthClean = 0;  // the chimney stones, 0..1
    int32_t debrisLeft = 0;  // pieces still inside the cabin
    float snow = 0;
    bool surveyed = false;
    bool cleared = false;  // the Clear step's goals are met right now (World::cleared latches it)

    // Repair
    int32_t planksLeft = 0;      // rotten or missing floorboards
    int32_t roofLeft = 0;        // sections of the hole still open
    int32_t boardsLeft = 0;      // boards still over the window
    bool chairMended = false;
    bool tableUpright = false;
    bool repairedNow = false;    // all of the above, and the old wood hauled out

    // Furnish
    int32_t furnished = 0;       // furnishings placed in the cabin

    // Kindle
    bool ashOut = false;
    bool kindlingLaid = false;
    bool logsLaid = false;       // at least one split log on the andirons (or already burning)
    bool woodStacked = false;    // the woodpile holds kWoodpileGoal or more
    bool burning = false;        // lit, and grown past a flicker
};

constexpr int kWoodpileGoal = 4;
constexpr float kBurningFire = 0.6f;

Progress progress(const World& w);

}  // namespace kindling
