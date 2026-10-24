#include "play/session.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <cstdio>

#include "save/save.h"
#include "sim/progress.h"
#include "sim/rules.h"

namespace kindling::play {

namespace {

// SDL3 scancodes (USB HID usage ids), which Strata reports as-is.
constexpr int kA = 4, kD = 7, kE = 8, kL = 15, kR = 21, kS = 22, kT = 23, kW = 26, k1 = 30, kEscape = 41, kGrave = 53,
              kF1 = 58, kF5 = 62, kF9 = 66, kF12 = 69, kRight = 79, kLeft = 80, kDown = 81, kUp = 82,
              kLShift = 225, kRShift = 229;
constexpr double kAutosaveSeconds = 120.0;
constexpr float kMouseSensitivity = 0.0025f;  // radians per pixel
// The arrow keys look around too (for a player without a mouse, and for
// playtesting agents): radians a second of game time, turning and up/down.
constexpr float kKeyTurnRate = 1.5707963f;
constexpr float kKeyPitchRate = 1.0f;

}  // namespace

Session::Session(strata::Engine& engine, const SessionOptions& options)
    : engine_(engine), options_(options), w_(newWorld(options.seed)), scene_(engine) {
    if (!options_.fresh) {
        const LoadResult r = loadSave(options_.slot, w_);
        if (r.ok || r.message != "No save to load.") messages_.push_back({r.message, 8.0});
        if (!r.ok) w_ = newWorld(options_.seed);
    }
    if (options_.browser) help_ = false;  // the page shows the controls first
    scene_.sync(w_);
    yaw_ = w_.player.yaw;
    pitch_ = w_.player.pitch;
    revealed_ = progress(w_).step == Step::Done;
    capture(true);
    if (!options_.playtestOut.empty()) {
        recorder_ = std::make_unique<playtest::Recorder>(options_.playtestOut, [] {
            return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count());
        });
        recorder_->worldStarted(w_, w_.tick > 0 ? "loaded" : "created");
    }
}

std::string Session::lastMessage() const { return messages_.empty() ? std::string() : messages_.back().text; }

bool Session::save(const char* note) {
    std::string why;
    const bool ok = writeSave(options_.slot, w_, &why);
    if (ok && note) messages_.push_back({note, 0});
    if (!ok) {
        messages_.push_back({"Could not save: " + why, 0});
        std::fprintf(stderr, "kindling: could not save: %s\n", why.c_str());
    }
    lastSave_ = clock_;
    if (ok) ++savesWritten_;
    if (ok && options_.afterSave) options_.afterSave();
    return ok;
}

void Session::load() {
    World loaded;
    const LoadResult r = loadSave(options_.slot, loaded);
    messages_.push_back({r.message, clock_ + 6.0});
    if (r.ok) {
        w_ = std::move(loaded);
        yaw_ = w_.player.yaw;
        pitch_ = w_.player.pitch;
        scene_.sync(w_);
        scene_.reloadGrime(w_);
        revealed_ = progress(w_).step == Step::Done;
        if (recorder_) recorder_->worldStarted(w_, "loaded");
    }
}

void Session::restart() {
    if (reveal_.active()) reveal_.finish(scene_, w_);
    w_ = newWorld(options_.seed);
    yaw_ = w_.player.yaw;
    pitch_ = w_.player.pitch;
    scene_.sync(w_);
    scene_.reloadGrime(w_);
    revealed_ = false;
    pilot_ = Autopilot();
    messages_.push_back({"A new start: the cabin as you found it.", clock_ + 6.0});
    if (recorder_) recorder_->worldStarted(w_, "created");
}

void Session::capture(bool on) {
    if (options_.browser) return;  // the page asks for the pointer lock on a click
    try {
        engine_.setRelativeMouseMode(on);
        captured_ = on;
    } catch (const strata::Error&) {
        captured_ = false;
    }
}

void Session::setCaptured(bool on) {
    captured_ = on;
    if (!on) mouseDown_ = false;
}

void Session::setHelp(bool on) {
    help_ = on;
    helpToggled_ = true;
}

void Session::skipReveal() {
    if (reveal_.active()) reveal_.finish(scene_, w_);
}

void Session::look(float dx, float dy) {
    yaw_ += dx * kMouseSensitivity;
    pitch_ = clampf(pitch_ - dy * kMouseSensitivity, -1.45f, 1.45f);
}

void Session::setStick(float forward, float strafe) {
    stickForward_ = clampf(forward, -1.0f, 1.0f);
    stickStrafe_ = clampf(strafe, -1.0f, 1.0f);
}

void Session::setUse(bool down) { touchUse_ = down; }

void Session::press(int scancode) { onKeyDown(scancode); }

void Session::onKeyDown(int scancode) {
    if (reveal_.active()) {
        if (scancode == kEscape) reveal_.finish(scene_, w_);
        return;
    }
    if (scancode == kEscape) {
        if (options_.browser) return;  // the browser itself releases the pointer lock
        if (captured_) capture(false);
        else running_ = false;
    } else if (scancode == kF1) {
        help_ = !help_;
        helpToggled_ = true;
    }
    else if (scancode == kF12 && !options_.browser) engine_.saveScreenshotPng("kindling-screenshot.png");
    else if (scancode == kE) interactPressed_ = true;
    else if (scancode == kR) rotatePressed_ += 1;
    else if (scancode == kL) lanternPressed_ = true;
    else if (scancode == kT && progress(w_).step == Step::Done) reveal_.start(clock_);
    else if (scancode == kF5) save("Saved.");
    else if (scancode == kF9) load();
    else if (scancode == kGrave) toolPressed_ = 0;
    else if (scancode >= k1 && scancode < k1 + kToolCount - 1) toolPressed_ = scancode - k1 + 1;
}

void Session::handle(const StrataEvent& e) {
    switch (e.type) {
        case STRATA_EVENT_QUIT: running_ = false; break;
        case STRATA_EVENT_KEY_DOWN:
            if (e.scancode >= 0 && e.scancode < kMaxScancode) keys_[e.scancode] = tapped_[e.scancode] = true;
            if (!e.repeat) onKeyDown(e.scancode);
            break;
        case STRATA_EVENT_KEY_UP:
            if (e.scancode >= 0 && e.scancode < kMaxScancode) keys_[e.scancode] = false;
            break;
        case STRATA_EVENT_MOUSE_BUTTON_DOWN:
            if (!captured_) capture(true);
            else if (e.button == 1) mouseDown_ = true;
            break;
        case STRATA_EVENT_MOUSE_BUTTON_UP:
            if (e.button == 1) mouseDown_ = false;
            break;
        case STRATA_EVENT_MOUSE_MOTION:
            if (captured_ && !options_.browser) look(e.dx, e.dy);
            break;
        case STRATA_EVENT_RESIZED:
            // In a browser the page sizes the canvas (kindling_resize); resizing
            // to the reported pixel size there would grow it by the device
            // pixel ratio on every event.
            if (!options_.browser) engine_.resize(e.width, e.height);
            break;
        default: break;
    }
}

// One tick of the rules (or `speed` of them, for the autopilot).
void Session::step() {
    for (int k = 0; k < (options_.autopilot ? options_.speed : 1); ++k) {
        Input in;
        if (options_.autopilot) {
            in = pilot_.next(w_);
        } else {
            if (captured_ || options_.browser) {
                yaw_ += kKeyTurnRate * kTickSeconds * ((held(kRight) ? 1.0f : 0.0f) - (held(kLeft) ? 1.0f : 0.0f));
                pitch_ = clampf(pitch_ + kKeyPitchRate * kTickSeconds * ((held(kUp) ? 1.0f : 0.0f) - (held(kDown) ? 1.0f : 0.0f)),
                                -1.45f, 1.45f);
            }
            in.forward = clampf((held(kW) ? 1.0f : 0.0f) - (held(kS) ? 1.0f : 0.0f) + stickForward_, -1.0f, 1.0f);
            in.strafe = clampf((held(kD) ? 1.0f : 0.0f) - (held(kA) ? 1.0f : 0.0f) + stickStrafe_, -1.0f, 1.0f);
            in.sprint = held(kLShift) || held(kRShift) ||
                        std::sqrt(stickForward_ * stickForward_ + stickStrafe_ * stickStrafe_) > 0.95f;
            in.yaw = yaw_;
            in.pitch = pitch_;
            in.use = (mouseDown_ && captured_) || touchUse_;
            in.interact = interactPressed_;
            in.selectTool = toolPressed_;
            in.rotate = rotatePressed_;
            in.toggleLantern = lanternPressed_;
            aim_ = aimFor(scene_, w_);
            in.aim = aim_;
        }
        // A key pressed and released between two frames still counts for one tick.
        std::fill(std::begin(tapped_), std::end(tapped_), false);
        lanternPressed_ = interactPressed_ = false;
        toolPressed_ = -1;
        rotatePressed_ = 0;
        TickEvents ev;
        tick(w_, in, &ev);
        if (options_.autopilot) {
            aim_ = in.aim;
            yaw_ = w_.player.yaw;
            pitch_ = w_.player.pitch;
        }
        scene_.updateGrime(w_, ev.grime);
        for (std::string& m : ev.messages) {
            if (recorder_) fresh_.push_back(m);
            // The same words again refresh the line already showing.
            bool shown = false;
            for (Message& old : messages_)
                if (old.text == m) {
                    old.until = clock_ + 6.0;
                    shown = true;
                }
            if (!shown) messages_.push_back({std::move(m), clock_ + 6.0});
        }
    }
    // Once Ada has settled, the reveal plays (once by itself; T plays it again).
    if (!revealed_ && progress(w_).step == Step::Done) {
        revealed_ = true;
        writeReplayFile(w_, options_.replayPath);
        reveal_.start(clock_ + 3.0);
    }
}

void Session::record(double dt) {
    if (!recorder_) return;
    fpsSeconds_ += dt;
    ++fpsFrames_;
    if (fpsSeconds_ >= 1.0) {
        fps_ = static_cast<int>(fpsFrames_ / fpsSeconds_ + 0.5);
        fpsSeconds_ = 0;
        fpsFrames_ = 0;
    }
    playtest::HostView v;
    v.screen = reveal_.active() ? "REVEAL" : "WORLD";
    v.fps = fps_;
    v.mouseCaptured = captured_;
    v.help = help_;
    v.clock = clockText(w_);
    const AimText at = aimText(w_, aim_);
    v.aim = at.name;
    v.aimHint = at.hint;
    v.aimInReach = at.inReach;
    v.aimNote = at.note;
    v.aimMeter = meterText(w_, at);
    for (const HudRow& row : checklist(w_)) v.checklist.emplace_back(row.done, row.text);
    v.carrying = carryLine(w_);
    for (const Message& m : messages_)
        if (m.until == 0 && std::find(fresh_.begin(), fresh_.end(), m.text) == fresh_.end()) fresh_.push_back(m.text);
    recorder_->update(w_, v, fresh_, savesWritten_);
    fresh_.clear();
}

void Session::frame(double dt) {
    record(dt);
    if (reveal_.active()) {
        clock_ += dt;
        if (reveal_.frame(engine_, scene_, w_, clock_)) return;
    }
    pending_ += dt;
    if (pending_ > 0.25) pending_ = 0.25;  // after a stall, do not try to catch up
    while (pending_ >= kTickSeconds) {
        pending_ -= kTickSeconds;
        clock_ += kTickSeconds;
        step();
    }
    if (clock_ - lastSave_ >= kAutosaveSeconds) save("Autosaved.");
    for (Message& m : messages_)
        if (m.until == 0) m.until = clock_ + 3.0;
    while (!messages_.empty() && messages_.front().until < clock_) messages_.erase(messages_.begin());
    if (messages_.size() > 3) messages_.erase(messages_.begin(), messages_.end() - 3);
    // The keys help the first steps; once the Survey is done it gets out of
    // the way (F1 brings it back).
    if (help_ && !helpToggled_ && progress(w_).step != Step::Survey) help_ = false;
    const Help help = !help_ ? Help::Hidden : options_.browser ? Help::Browser : Help::Desktop;
    render(engine_, scene_, w_, aim_, help, messages_, clock_, compact_);
}

}  // namespace kindling::play
