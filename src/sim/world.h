// The whole saved state of a game of Kindling, as plain data.
//
// The rules (rules.h) change it one tick at a time from an Input; nothing here
// touches the engine, so the simulation builds and tests without a GPU. Every
// field is part of the state fingerprint, and later of the save file.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "sim/grime.h"
#include "sim/items.h"
#include "sim/math.h"
#include "sim/target.h"

namespace kindling {

constexpr int kTickRate = 30;                 // simulation ticks per real second
constexpr float kTickSeconds = 1.0f / kTickRate;

// ---- what the ruin starts with

enum class DebrisKind : int32_t { Board = 0, Shingle = 1, Rafter = 2, RafterPiece = 3 };

struct Debris {
    int32_t id = 0;
    DebrisKind kind = DebrisKind::Board;
    Vec3 pos;           // centre, on the floor
    float yaw = 0;      // radians about +y
    bool hauled = false;  // carried out of the cabin
};

enum class PlankState : int32_t { Rotten = 0, Removed = 1, New = 2 };
enum class ChairState : int32_t { Broken = 0, Mended = 1 };
enum class TableState : int32_t { Toppled = 0, Upright = 1 };

struct Player {
    float x = 0, z = 0;
    float yaw = 0, pitch = 0;  // radians; yaw 0 faces -z
};

// The tools on the player's belt, chosen with the number keys.
enum class Tool : int32_t {
    None = 0,
    Broom,       // sweeps the floor
    ScrubBrush,  // scrubs walls and the chimney stones
    Shovel,      // snow, ash
    Saw,         // cuts the fallen rafter into pieces you can carry
    PryBar,      // lifts rotten boards and the boards over the window
    Hammer,      // mends the chair
    Count
};
constexpr int kToolCount = static_cast<int>(Tool::Count);
const char* toolName(Tool t);

// What the player's hands are full of, if anything.
enum class Carry : int32_t { Nothing = 0, Debris = 1, Item = 2 };

// Kinds of damage the survey asks the player to find, as bits of World::surveyed.
constexpr uint32_t kSurveyGrime = 1u << 0;
constexpr uint32_t kSurveyDebris = 1u << 1;
constexpr uint32_t kSurveyRottenPlank = 1u << 2;
constexpr uint32_t kSurveyRoofHole = 1u << 3;
constexpr uint32_t kSurveyBoardedWindow = 1u << 4;
constexpr uint32_t kSurveyAll =
    kSurveyGrime | kSurveyDebris | kSurveyRottenPlank | kSurveyRoofHole | kSurveyBoardedWindow;

struct World {
    int64_t tick = 0;
    uint64_t seed = 0;
    Player player;
    Tool tool = Tool::None;
    Carry carry = Carry::Nothing;
    int32_t carryIndex = -1;  // into debris or items, by what is carried

    // A job that takes a while (sawing, shovelling): what it is aimed at and how
    // far along it is. Looking away or letting go resets it.
    TargetKind workKind = TargetKind::None;
    int32_t workIndex = -1;
    float workSeconds = 0;

    std::vector<Debris> debris;
    std::array<PlankState, 3> planks{};         // one per layout::kRottenSpans entry
    std::array<int32_t, 4> windowBoards{};      // 1 while nailed over the window
    uint32_t roofPatched = 0;                   // bit k: section k (0..3) of the roof hole patched
    ChairState chair = ChairState::Broken;
    TableState table = TableState::Toppled;
    float snow = 1.0f;                          // the drift under the roof hole, 0..1
    int32_t debrisHauled = 0;                   // pieces carried out to the pile in the yard
    Grime grime;
    std::vector<Item> items;                    // materials and furnishings, from the wagon

    // Steps once finished stay finished, even if the player makes a new mess.
    bool cleared = false;
    bool repaired = false;

    uint32_t surveyed = 0;                      // kSurvey* bits the player has looked at
};

constexpr uint32_t kRoofAllPatched = 0xF;

// A new game: the ruin as the player finds it, the player in the yard.
World newWorld(uint64_t seed);

// 64-bit FNV-1a over every saved field in a fixed order. Two worlds with the
// same fingerprint are the same world.
uint64_t fingerprint(const World& w);

}  // namespace kindling
