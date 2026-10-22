#include "play/session.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "save/save.h"
#include "sim/progress.h"
#include "sim/rules.h"

namespace kindling::play {

namespace {

// SDL3 scancodes (USB HID usage ids), which Strata reports as-is.
constexpr int kA = 4, kD = 7, kE = 8, kL = 15, kR = 21, kS = 22, kT = 23, kW = 26, k1 = 30, kEscape = 41, kGrave = 53,
              kF1 = 58, kF5 = 62, kF9 = 66, kF12 = 69, kLShift = 225, kRShift = 229;
constexpr double kAutosaveSeconds = 120.0;
constexpr float kMouseSensitivity = 0.0025f;  // radians per pixel

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

void Session::setHelp(bool on) { help_ = on; }

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
    } else if (scancode == kF1) help_ = !help_;
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
            if (e.scancode >= 0 && e.scancode < kMaxScancode) keys_[e.scancode] = true;
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
            in.forward = clampf((keys_[kW] ? 1.0f : 0.0f) - (keys_[kS] ? 1.0f : 0.0f) + stickForward_, -1.0f, 1.0f);
            in.strafe = clampf((keys_[kD] ? 1.0f : 0.0f) - (keys_[kA] ? 1.0f : 0.0f) + stickStrafe_, -1.0f, 1.0f);
            in.sprint = keys_[kLShift] || keys_[kRShift] ||
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
        for (std::string& m : ev.messages) messages_.push_back({std::move(m), clock_ + 6.0});
    }
    // Once Ada has settled, the reveal plays (once by itself; T plays it again).
    if (!revealed_ && progress(w_).step == Step::Done) {
        revealed_ = true;
        writeReplayFile(w_, options_.replayPath);
        reveal_.start(clock_ + 3.0);
    }
}

void Session::frame(double dt) {
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
    const Help help = !help_ ? Help::Hidden : options_.browser ? Help::Browser : Help::Desktop;
    render(engine_, scene_, w_, aim_, help, messages_, clock_, compact_);
}

}  // namespace kindling::play
