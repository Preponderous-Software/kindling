// What the player sees: the HUD, the camera, the sky, and the reveal (the
// restoration played back from its keyframes). Shared by the desktop game
// (app/main.cpp) and the browser game (app/web_main.cpp).
#pragma once

#include <string>
#include <vector>

#include "bridge/scene.h"
#include "sim/target.h"
#include "sim/world.h"
#include "strata/strata.hpp"

namespace kindling::play {

// A line of news under the crosshair ("Saved.", "The fire caught.").
struct Message {
    std::string text;
    double until = 0;  // seconds of play time; 0 = not shown yet
};

// The controls list in the corner: hidden, or for the desktop or the browser
// game (the browser has no quit and no F12 screenshot).
enum class Help { Hidden, Desktop, Browser };

// The sun, its shadows and fog. `lighter` uses fewer, smaller shadow maps,
// for the browser.
void setLighting(strata::Engine& engine, bool lighter = false);
// The sun (or the moon's sky) at the game's time; returns daylight, 0 at night
// to 1 with the sun well up.
float setSky(strata::Engine& engine, const World& w);
std::string clockText(const World& w);
StrataCamera playerCamera(const Player& p, float aspect);

// One first-person frame with its HUD (begin..endFrame; the caller presents).
// A compact HUD draws no words at all: a narrow browser page shows the
// crosshair, the checklist and the rest as page text, from the functions below.
void render(strata::Engine& engine, bridge::Scene& scene, const World& w, const Aim& aim, Help help,
            const std::vector<Message>& messages, double seconds, bool compact = false);

// The HUD's words. The checklist for the step the player is on: the step's
// name, its items (done or not), notes, and Ada's brief once it matters.
struct HudRow {
    enum Kind { Step, Item, Note, Brief } kind;
    bool done;
    std::string text;
};
std::vector<HudRow> checklist(const World& w);
// What the crosshair is on, when it is near enough to name.
struct AimText {
    std::string name, hint;
    bool inReach = false;  // close enough to work on
};
AimText aimText(const World& w, const Aim& aim);
// What the player is carrying and what to do with it; empty when nothing.
std::string carryLine(const World& w);
// What the crosshair is on.
Aim aimFor(const bridge::Scene& scene, const World& w);

// The restoration played back from the world's keyframes, with a slow camera
// drift across the room.
class Reveal {
public:
    bool active() const { return active_; }
    void start(double now);
    // Draws the reveal at `now`; false once it has finished (the scene is then
    // back on the real world).
    bool frame(strata::Engine& engine, bridge::Scene& scene, const World& w, double now);
    void finish(bridge::Scene& scene, const World& w);

private:
    bool active_ = false;
    double started_ = 0;
    size_t index_ = 0;
    World ghost_;
};

// Writes the restoration as a usd-replay/1 file; nothing when `path` is empty.
void writeReplayFile(const World& w, const std::string& path);

}  // namespace kindling::play
