// A game being played: the world, its save slot, the input, the autosave and
// the reveal, one frame at a time. The desktop game (app/main.cpp) drives it
// from its own loop; the browser game (app/web_main.cpp) from the browser's.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "bridge/scene.h"
#include "play/hud.h"
#include "playtest/probe.h"
#include "sim/autopilot.h"
#include "sim/world.h"
#include "strata/strata.hpp"

namespace kindling::play {

struct SessionOptions {
    uint64_t seed = 1881;
    std::string slot;        // the save slot directory
    std::string replayPath;  // where the restoration replay is written at the end; empty = nowhere
    bool fresh = false;      // start a new game even if there is a save
    bool autopilot = false;  // the autopilot plays (watch mode)
    int speed = 1;           // autopilot ticks per tick
    // In a browser the page owns the pointer lock and turns the mouse into
    // look() calls; Esc never quits and there is no F12 screenshot.
    bool browser = false;
    // Called after every save written (the browser flushes it to IndexedDB).
    std::function<void()> afterSave;
    // --playtest-out: where to write state.json and events.jsonl for an AI
    // playtester (src/playtest/probe.h); empty = nothing is written.
    std::string playtestOut;
};

class Session {
public:
    Session(strata::Engine& engine, const SessionOptions& options);

    // An event from the engine.
    void handle(const StrataEvent& e);
    // Advances by `dt` seconds of real time and draws a frame (not presented).
    void frame(double dt);
    bool running() const { return running_; }

    // Writes the save now; `note` is shown on success.
    bool save(const char* note);
    // Loads the last save, if it can.
    void load();
    // Starts over: a new world (the next save replaces the current one, which
    // is then kept as the previous save).
    void restart();

    // Touch and page input (the browser): look by a pixel delta, walk with a
    // stick (-1..1 each, beyond 0.95 hurries), hold to work, press a key.
    void look(float dx, float dy);
    void setStick(float forward, float strafe);
    void setUse(bool down);
    void press(int scancode);
    // The page took or released the pointer lock.
    void setCaptured(bool on);
    void setHelp(bool on);
    // No words on the canvas (a narrow page shows them itself).
    void setCompactHud(bool on) { compact_ = on; }
    void skipReveal();

    const World& world() const { return w_; }
    const Aim& aim() const { return aim_; }
    bool captured() const { return captured_; }
    bool help() const { return help_; }
    bool revealing() const { return reveal_.active(); }
    // The newest message, for the page.
    std::string lastMessage() const;

private:
    void capture(bool on);
    void onKeyDown(int scancode);
    void step();
    void record(double dt);

    strata::Engine& engine_;
    SessionOptions options_;
    World w_;
    bridge::Scene scene_;
    Autopilot pilot_;
    Reveal reveal_;
    std::vector<Message> messages_;
    static constexpr int kMaxScancode = 512;
    bool keys_[kMaxScancode] = {};
    bool tapped_[kMaxScancode] = {};  // went down since the last tick
    bool held(int scancode) const { return keys_[scancode] || tapped_[scancode]; }
    bool captured_ = false, help_ = true, running_ = true, mouseDown_ = false, touchUse_ = false;
    bool helpToggled_ = false;  // F1 or the page chose; otherwise it tucks away after the Survey
    bool interactPressed_ = false, lanternPressed_ = false;
    int32_t toolPressed_ = -1, rotatePressed_ = 0;
    float yaw_ = 0, pitch_ = 0;
    float stickForward_ = 0, stickStrafe_ = 0;
    Aim aim_;
    double clock_ = 0, lastSave_ = 0, pending_ = 0;
    bool revealed_ = false;
    bool compact_ = false;
    // The playtest probe, when asked for.
    std::unique_ptr<playtest::Recorder> recorder_;
    std::vector<std::string> fresh_;  // messages since the recorder last ran
    int savesWritten_ = 0;
    double fpsSeconds_ = 0;
    int fpsFrames_ = 0, fps_ = 0;
};

}  // namespace kindling::play
