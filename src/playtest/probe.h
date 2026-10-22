// --playtest-out DIR: what an AI playtester reads instead of guessing from
// screenshots (ARCHITECTURE.md, "Playtesting by agents"):
//
//   DIR/state.json    the game as the player sees it, rewritten about twice a
//                     second through a temporary file renamed over the old one,
//                     so a reader never sees half a file
//   DIR/events.jsonl  one JSON object a line, appended as things happen:
//                     screens, the world starting or loading, each restoration
//                     step completed, every message, saves
//
// No engine here: the host (play::Session, or the headless autopilot) passes
// what only it knows (the screen, the frame rate, the HUD's words) in a
// HostView. The recorder only reads the World, so the game behaves exactly the
// same with the flag as without it. A file that cannot be written is reported
// once on stderr and otherwise ignored: a playtest aid must never stop a game.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "sim/world.h"

namespace kindling::playtest {

// What the host shows that the World does not know.
struct HostView {
    std::string screen = "WORLD";  // WORLD, or REVEAL while the timelapse plays
    int fps = 0;
    bool mouseCaptured = true;     // false after Esc freed the mouse
    bool help = false;             // the controls list is showing
    std::string clock;             // "9:01 am", as the HUD shows it
    std::string aim;               // what the crosshair is on, when near enough to name
    std::string aimHint;
    bool aimInReach = false;
    std::vector<std::pair<bool, std::string>> checklist;  // the HUD's rows: done?, text
    std::string carrying;          // the carry line, empty when hands are free
};

// Something the player can walk to: what state.json's "nearest" lists.
struct Target {
    std::string kind;   // door, wagon, hearth, window, chopping_block, mare, debris
    std::string name;
    std::string id;
    float distance = 0;  // metres, to the nearest point of it on the ground plane
    float bearing = 0;   // degrees from where the player faces, + to the right
};

// The kinds "nearest" reports, in the order they are written.
const std::vector<std::string>& targetKinds();
// Up to `perKind` of each kind, closest first.
std::vector<Target> nearest(const World& w, int perKind = 3);
// The player's heading in degrees, 0 facing -z (toward the cabin), growing as
// the player turns right; 0 <= heading < 360.
float headingDegrees(float yaw);
// Degrees from where the player faces to the point (x, z), + to the right.
float bearingTo(const Player& p, float x, float z);

// The version the game reports (the CMake project version).
const char* gameVersion();

// The step names for objective_completed events: "Survey" ... "Welcome".
std::string stepTitle(int32_t step);

std::string stateJson(const World& w, const HostView& view, int64_t wallTimeMs, double ticksPerSecond);
std::string eventJson(const std::string& type, int64_t tick, int64_t wallTimeMs,
                      const std::vector<std::pair<std::string, std::string>>& rawFields);
// A JSON string literal for `text`, quotes included.
std::string quote(const std::string& text);

class Recorder {
public:
    // `clockMs` gives wall time in Unix milliseconds (tests pass their own).
    Recorder(std::string directory, std::function<int64_t()> clockMs, int stateIntervalMs = 500);

    // A world was created ("created") or loaded from a save ("loaded").
    void worldStarted(const World& w, const char* how);
    // Once a frame, after the tick(s): what the host shows, the messages the
    // rules produced since the last call, and the saves written so far.
    void update(const World& w, const HostView& view, const std::vector<std::string>& messages, int savesWritten);

    const std::string& directory() const { return dir_; }

private:
    void append(const std::string& type, int64_t tick, const std::vector<std::pair<std::string, std::string>>& fields);
    void writeState(const std::string& json);
    void failed(const std::string& what);

    std::string dir_;
    std::function<int64_t()> clockMs_;
    int intervalMs_;
    int64_t nextStateMs_ = 0;
    int64_t lastStateMs_ = -1;
    int64_t lastStateTick_ = 0;
    double tps_ = 0;
    bool primed_ = false;
    std::string screen_;
    int32_t step_ = 0;
    int64_t stepTick_ = 0;
    int saves_ = 0;
    bool reportedFailure_ = false;
};

}  // namespace kindling::playtest
