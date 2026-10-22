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

// The clock: one game minute per real second, from nine in the morning on
// 11 November (late autumn) of the first year.
constexpr int kStartMinute = 9 * 60;
constexpr float kGameMinutesPerSecond = 1.0f;

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
    Axe,         // splits log rounds on the chopping block
    Matches,     // lights the hearth and the candle
    CurryComb,   // brushes the mare
    Count
};
constexpr int kToolCount = static_cast<int>(Tool::Count);
const char* toolName(Tool t);

// What the player's hands are full of, if anything.
enum class Carry : int32_t { Nothing = 0, Debris = 1, Item = 2, Firewood = 3, Hay = 4 };

// The mare in the stall. She is never in danger: hungry or dusty only changes
// how she greets you.
struct Horse {
    float dirt = 0.8f;          // 0 gleaming .. 1 caked from the trail
    float hunger = 0.7f;        // 0 just fed .. 1 hungry
    float eatingSeconds = 0;    // munching hay, head down, after being fed
    float brushedSeconds = 0;   // the last few seconds of brushing, for leaning into it
    bool greeted = false;       // she has nickered at you
};

// Firewood in its forms; carried as Carry::Firewood with the kind in carryIndex.
enum class Wood : int32_t { Round = 0, Split = 1, Kindling = 2 };
const char* woodName(Wood k);  // "a log round"

// The hearth, from cold ash to a fire that keeps the room warm.
struct Hearth {
    float ash = 1.0f;          // old ash still in the firebox, 0..1
    bool kindlingLaid = false;
    int32_t logsLaid = 0;      // split logs on the andirons, unburned
    bool lit = false;
    float fire = 0;            // 0..1, how big the fire is
    float fuelMinutes = 0;     // game minutes of burning left in what is laid
    bool candleLit = false;
    int32_t matchesStruck = 0;
};

// Firewood around the yard and the hearth.
struct Firewood {
    int32_t rounds = 6;        // unsplit rounds on the pile by the chopping block
    bool roundOnBlock = false;
    int32_t splitByBlock = 0;  // split logs lying by the block
    int32_t kindlingByBlock = 0;
    int32_t woodpile = 2;      // split logs stacked inside, left of the hearth (two old ones to start)
};

// Kinds of damage the survey asks the player to find, as bits of World::surveyed.
constexpr uint32_t kSurveyGrime = 1u << 0;
constexpr uint32_t kSurveyDebris = 1u << 1;
constexpr uint32_t kSurveyRottenPlank = 1u << 2;
constexpr uint32_t kSurveyRoofHole = 1u << 3;
constexpr uint32_t kSurveyBoardedWindow = 1u << 4;
constexpr uint32_t kSurveyAll =
    kSurveyGrime | kSurveyDebris | kSurveyRottenPlank | kSurveyRoofHole | kSurveyBoardedWindow;

// Ada Lark, the settler this cabin is being made ready for.
enum class SettlerState : int32_t { Away = 0, Arriving = 1, LookingAround = 2, ToChair = 3, Seated = 4 };

struct Settler {
    SettlerState state = SettlerState::Away;
    float x = 2.5f, z = 10.5f;  // starts at the wagon
    float yaw = 0;
    int32_t waypoint = 0;       // along her walk in
    int32_t line = -1;          // the line she is saying, or -1
    float lineSeconds = 0;      // how long it has been showing
    bool briefMet = false;      // what she found, fixed when she arrives
    bool done = false;          // every line said, sitting by the fire
};

// A snapshot of the cabin taken while the player works, for the reveal: just
// what can be seen, at a coarser grime resolution than the game's.
struct Keyframe {
    int64_t tick = 0;
    int32_t step = 0;  // the Step the player was on (sim/progress.h)
    float playerX = 0, playerZ = 0, playerYaw = 0;
    std::array<std::vector<uint8_t>, kSurfaceCount> grime;  // each surface at half resolution
    std::vector<Debris> debris;
    std::array<PlankState, 3> planks{};
    std::array<int32_t, 4> windowBoards{};
    uint32_t roofPatched = 0;
    ChairState chair = ChairState::Broken;
    TableState table = TableState::Toppled;
    float snow = 0;
    std::vector<Item> items;
    Hearth hearth;
    int32_t woodpile = 0;
    Settler settler;
    bool lantern = false;
};

constexpr int kKeyframeEveryTicks = 10 * kTickRate;  // and at every step change

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

    Hearth hearth;
    Firewood wood;
    bool lantern = false;  // carrying a lit lantern (for the dark)
    Horse horse;

    Settler settler;
    std::vector<Keyframe> history;

    // Steps once finished stay finished, even if the player makes a new mess.
    bool cleared = false;
    bool repaired = false;
    bool kindled = false;

    uint32_t surveyed = 0;                      // kSurvey* bits the player has looked at
};

constexpr uint32_t kRoofAllPatched = 0xF;

// Game minutes since midnight on the first day (11 November).
double gameMinutes(const World& w);
// 0..1 through the day (0.5 is noon), and the day number (0 = 11 November).
double timeOfDay(const World& w);
int32_t dayNumber(const World& w);
// The first frost settles after dark on the first night.
bool frosted(const World& w);

// A new game: the ruin as the player finds it, the player in the yard.
World newWorld(uint64_t seed);

// 64-bit FNV-1a over every saved field in a fixed order. Two worlds with the
// same fingerprint are the same world.
uint64_t fingerprint(const World& w);

// A snapshot of what can be seen now, and a world that looks like a snapshot
// (for the reveal: built on a copy of `base`, grime scaled back up).
Keyframe snapshot(const World& w);
World fromKeyframe(const World& base, const Keyframe& k);

}  // namespace kindling
