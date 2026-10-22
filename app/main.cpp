// kindling: the game on the Strata engine.
//
//   kindling [--seed N] [--width W --height H] [--autopilot [--speed N]] [--replay PATH]
//   kindling --headless --tour --frames DIR [--width W --height H]
//   kindling --headless --autopilot [--frames DIR --frame-every N] [--reveal-frames DIR] [--replay PATH]
//
// Controls: mouse to look, W A S D to walk, Shift to hurry, 1-8 to take a tool
// (` for bare hands), hold the left button to work, E to pick up or put down,
// R to turn what you carry, L for the lantern, F1 for help, Esc to free the
// mouse (Esc again quits). --tour plays a short
// scripted visit and, headless, writes a screenshot at each stop to DIR.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bridge/scene.h"
#include "record/replay.h"
#include "sim/autopilot.h"
#include "sim/brief.h"
#include "sim/settler.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/rules.h"
#include "strata/strata.hpp"

using namespace kindling;
namespace L = kindling::layout;

namespace {

// SDL3 scancodes (USB HID usage ids), which Strata reports as-is.
constexpr int kA = 4, kD = 7, kE = 8, kL = 15, kR = 21, kS = 22, kT = 23, kW = 26, k1 = 30, kEscape = 41, kGrave = 53, kF1 = 58,
              kF12 = 69, kLShift = 225, kRShift = 229;
constexpr int kMaxScancode = 512;
constexpr float kMouseSensitivity = 0.0025f;  // radians per pixel
constexpr float kFovY = 70.0f;

struct Args {
    uint64_t seed = 1881;
    bool headless = false;
    bool tour = false;
    bool autopilot = false;
    int speed = 1;             // autopilot ticks per frame in a window
    std::string framesDir;
    int frameEvery = 300;      // headless autopilot: ticks between frames
    std::string revealFramesDir;
    std::string replayPath = "kindling-restoration.usda";
    int width = 1280, height = 720;
};

bool parseArgs(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        const std::string s = argv[i];
        auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "kindling: %s needs a value\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (s == "--seed") a.seed = std::strtoull(next("--seed"), nullptr, 10);
        else if (s == "--headless") a.headless = true;
        else if (s == "--tour") a.tour = true;
        else if (s == "--frames") a.framesDir = next("--frames");
        else if (s == "--autopilot") a.autopilot = true;
        else if (s == "--speed") a.speed = std::max(1, std::atoi(next("--speed")));
        else if (s == "--frame-every") a.frameEvery = std::max(1, std::atoi(next("--frame-every")));
        else if (s == "--reveal-frames") a.revealFramesDir = next("--reveal-frames");
        else if (s == "--replay") a.replayPath = next("--replay");
        else if (s == "--width") a.width = std::atoi(next("--width"));
        else if (s == "--height") a.height = std::atoi(next("--height"));
        else if (s == "--help" || s == "-h") return false;
        else {
            std::fprintf(stderr, "kindling: unknown argument '%s'\n", s.c_str());
            return false;
        }
    }
    if (a.headless && !a.tour && !a.autopilot) {
        std::fprintf(stderr, "kindling: --headless needs --tour or --autopilot (there is no one to play)\n");
        return false;
    }
    return true;
}

void usage() {
    std::puts("usage: kindling [--seed N] [--width W --height H] [--autopilot [--speed N]] [--replay PATH]\n"
              "       kindling --headless --tour --frames DIR [--width W --height H]\n"
              "       kindling --headless --autopilot [--frames DIR --frame-every N] [--reveal-frames DIR]\n"
              "                [--replay PATH]");
}

void setLighting(strata::Engine& engine) {
    StrataLighting light = strata::lightingDefaults(STRATA_LIGHTING_SUN);
    light.cascade_count = 4;
    light.shadow_map_size = 2048;
    light.shadow_distance = 45.0f;
    light.fog = 1;
    engine.setLighting(light);
}

// The sun (or the moon's sky) at the game's time, over a Montana valley in
// November. 2026 is used for the sky's geometry only; the date is what matters.
// Returns daylight, 0 at night to 1 with the sun well up.
float setSky(strata::Engine& engine, const World& w) {
    const double midnightUtc = 1794380400.0;  // 2026-11-11 00:00 at UTC-7
    const StrataSolarPosition sun = strata::solarPosition(midnightUtc + gameMinutes(w) * 60.0, 46.6, -112.0);
    engine.setSunDirection(sun.to_sun);
    return clampf((sun.elevation_degrees + 4.0f) / 16.0f, 0.0f, 1.0f);
}

std::string clockText(const World& w) {
    const int day = dayNumber(w);
    const int minute = static_cast<int>(timeOfDay(w) * 1440.0);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d November  %d:%02d %s", 11 + day, (minute / 60 + 11) % 12 + 1, minute % 60,
                  minute < 720 ? "am" : "pm");
    return buf;
}

StrataCamera playerCamera(const Player& p, float aspect) {
    const Vec3 d = lookDirection(p.yaw, p.pitch);
    const float eye[3] = {p.x, L::kEyeHeight, p.z};
    const float target[3] = {p.x + d.x, L::kEyeHeight + d.y, p.z + d.z};
    const float up[3] = {0, 1, 0};
    return strata::cameraLookAt(eye, target, up, kFovY, aspect, 0.05f, 400.0f);
}

const float kWhite[4] = {0.92f, 0.88f, 0.80f, 1.0f};
const float kDim[4] = {0.62f, 0.60f, 0.56f, 1.0f};
const float kEmber[4] = {0.95f, 0.45f, 0.14f, 1.0f};

// Strata's 8x8 font draws each glyph in a square cell `size` pixels wide.
float centred(float cx, float size, const std::string& s) { return cx - 0.5f * size * static_cast<float>(s.size()); }

void text(std::vector<StrataText>& out, float x, float y, float size, const float color[4], const std::string& s,
          std::vector<std::string>& storage) {
    storage.push_back(s);
    StrataText t{};
    t.x = x;
    t.y = y;
    t.pixel_size = size;
    t.flags = STRATA_TEXT_SHADOW;
    for (int i = 0; i < 4; ++i) t.color[i] = color[i];
    out.push_back(t);
}

struct Message {
    std::string text;
    double until = 0;  // seconds of play time
};

std::string percent(float f) { return std::to_string(static_cast<int>(f * 100.0f + 0.5f)) + "%"; }

void drawHud(strata::Engine& engine, const World& w, const Aim& aim, int width, int height, bool help,
             const std::vector<Message>& messages) {
    std::vector<StrataText> texts;
    std::vector<std::string> storage;
    storage.reserve(48);
    const float cx = width * 0.5f, cy = height * 0.5f;
    text(texts, cx - 6, cy - 8, 16, kWhite, "+", storage);

    const Description d = describe(w, aim);
    if (!d.name.empty() && aim.distance <= L::kReach * 2.5f) {
        const float* c = aim.distance <= L::kReach ? kWhite : kDim;
        text(texts, centred(cx, 18, d.name), cy + 28, 18, c, d.name, storage);
        if (!d.hint.empty())
            text(texts, centred(cx, 14, d.hint), cy + 52, 14, kDim, d.hint, storage);
    }

    // The checklist for the step the player is on.
    const Progress p = progress(w);
    float y = 20;
    text(texts, 20, y, 18, kEmber, std::string("STEP: ") + stepName(p.step), storage);
    y += 30;
    auto row = [&](bool done, const std::string& label) {
        text(texts, 20, y, 16, done ? kWhite : kDim, std::string(done ? "[x] " : "[ ] ") + label, storage);
        y += 24;
    };
    if (p.step == Step::Survey) {
        row(w.surveyed & kSurveyGrime, "Grime");
        row(w.surveyed & kSurveyDebris, "Debris");
        row(w.surveyed & kSurveyRottenPlank, "Rotten floorboards");
        row(w.surveyed & kSurveyRoofHole, "Hole in the roof");
        row(w.surveyed & kSurveyBoardedWindow, "Boarded window");
    } else if (p.step == Step::Clear) {
        row(p.debrisLeft == 0, "Haul out the debris (" + std::to_string(p.debrisLeft) + " left)");
        row(p.snow <= 0, "Shovel out the snow");
        row(p.floorClean >= kFloorCleanGoal, "Sweep the floor (" + percent(p.floorClean) + ")");
        row(p.wallsClean >= kWallCleanGoal, "Scrub the walls (" + percent(p.wallsClean) + ")");
        row(p.hearthClean >= kWallCleanGoal, "Scrub the chimney (" + percent(p.hearthClean) + ")");
    } else if (p.step == Step::Kindle) {
        row(p.woodStacked, "Split firewood and stack it by the hearth (" + std::to_string(w.wood.woodpile) + " of " +
                               std::to_string(kWoodpileGoal) + ")");
        row(p.ashOut, "Shovel out the old ash");
        row(p.kindlingLaid, "Lay kindling in the hearth");
        row(p.logsLaid, "Lay a split log on the andirons");
        row(p.burning, "Strike a match and hold it till it catches");
    } else if (p.step == Step::Repair) {
        row(p.planksLeft == 0, "Replace the rotten floorboards (" + std::to_string(p.planksLeft) + " left)");
        row(p.roofLeft == 0, "Patch the roof (" + std::to_string(p.roofLeft) + " sections open)");
        row(p.boardsLeft == 0, "Take the boards off the window");
        row(p.chairMended, "Mend the rocking chair");
        row(p.tableUpright, "Set the table right");
        row(p.debrisLeft == 0, "Haul out the old wood (" + std::to_string(p.debrisLeft) + " pieces)");
    } else if (p.step == Step::Furnish || p.step >= Step::Welcome) {
        if (p.step == Step::Furnish) {
            row(p.furnished >= kFurnishGoal, "Bring in furnishings (" + std::to_string(p.furnished) + " of " +
                                                 std::to_string(kFurnishGoal) + ")");
        } else {
            row(w.settler.state != SettlerState::Away, "Ada arrives");
            row(w.settler.state == SettlerState::Seated, "She settles by the fire");
            if (p.step == Step::Done) {
                text(texts, 20, y, 16, kDim, "T plays back your restoration.", storage);
                y += 24;
            }
        }
        y += 8;
        const Brief b = evaluateBrief(w);
        text(texts, 20, y, 16, kEmber, b.met() ? "ADA'S BRIEF: ALL MET" : "ADA'S BRIEF", storage);
        y += 26;
        for (const BriefPoint& bp : b.points) row(bp.met, bp.wish);
    }

    const std::string clock = clockText(w);
    text(texts, static_cast<float>(width) - 16.0f * static_cast<float>(clock.size()) - 20, 20, 16, kDim, clock, storage);

    // The tool belt, and what is in hand.
    std::string belt;
    for (int t = 1; t < kToolCount; ++t) {
        const bool held = static_cast<int>(w.tool) == t;
        belt += (held ? "[" : " ") + std::to_string(t) + " " + toolName(static_cast<Tool>(t)) + (held ? "]" : " ") + " ";
    }
    text(texts, centred(cx, 12, belt), static_cast<float>(height) - 36, 12, kWhite, belt, storage);
    if (w.carry == Carry::Debris)
        {
        const std::string line = "Carrying: E to put down (outside to haul out)";
        text(texts, centred(cx, 14, line), static_cast<float>(height) - 64, 14, kEmber, line, storage);
    }
    if (w.carry == Carry::Firewood) {
        const std::string line = std::string("Carrying ") + woodName(static_cast<Wood>(w.carryIndex)) + ": E to put it where it goes";
        text(texts, centred(cx, 14, line), static_cast<float>(height) - 64, 14, kEmber, line, storage);
    }
    if (w.carry == Carry::Item) {
        const Item& it = w.items[static_cast<size_t>(w.carryIndex)];
        const std::string line = std::string("Carrying ") + itemName(it.kind) +
                                 (isMaterial(it.kind) ? ": aim where it goes and hold the left button"
                                                      : ": E to put it down, R to turn it, E at the wagon to put it back");
        text(texts, centred(cx, 14, line), static_cast<float>(height) - 64, 14, kEmber, line, storage);
    }

    const std::string speech = settlerSpeech(w);
    if (!speech.empty()) {
        const std::string line = "Ada: \"" + speech + "\"";
        text(texts, cx - 4.5f * static_cast<float>(line.size()), static_cast<float>(height) - 110, 18, kWhite, line, storage);
    }

    float my = cy + 90;
    for (const Message& m : messages) {
        text(texts, centred(cx, 14, m.text), my, 14, kEmber, m.text, storage);
        my += 22;
    }

    if (help) {
        const char* lines[] = {"Mouse      look",       "W A S D    walk",          "Shift      hurry",
                               "1-6        take a tool", "Left mouse work with it", "E          pick up / put down",
                               "R          turn what you carry", "L          lantern",
                               "Esc        free the mouse / quit", "F1  hide help   F12  screenshot"};
        float hy = static_cast<float>(height) - 24 * 10 - 80;
        for (const char* l : lines) {
            text(texts, 20, hy, 16, kDim, l, storage);
            hy += 24;
        }
    }
    for (size_t i = 0; i < texts.size(); ++i) texts[i].utf8 = storage[i].c_str();
    engine.submitText(texts.data(), texts.size());
}

// While carrying a furnishing: an outline where it would go, if it can go there.
void drawPlacement(strata::Engine& engine, const World& w, const Aim& aim) {
    if (w.carry != Carry::Item) return;
    const Item& it = w.items[static_cast<size_t>(w.carryIndex)];
    if (isMaterial(it.kind)) return;
    Pose pose;
    if (!placementFor(w, it.kind, aim, it.yaw, pose)) return;
    float hx, hz;
    bridge::itemFootprint(it.kind, hx, hz);
    const float c = std::cos(pose.yaw), s = std::sin(pose.yaw);
    const bool upright = anchorOf(it.kind) == Anchor::Wall || anchorOf(it.kind) == Anchor::Chair;
    Vec3 corners[4];
    const float sx[4] = {-1, 1, 1, -1}, sz[4] = {-1, -1, 1, 1};
    for (int k = 0; k < 4; ++k) {
        const float lx = sx[k] * hx, lz = sz[k] * hz;
        corners[k] = upright ? Vec3{pose.pos.x + c * lx, pose.pos.y + lz, pose.pos.z - s * lx}
                             : Vec3{pose.pos.x + c * lx + s * lz, pose.pos.y + 0.02f, pose.pos.z - s * lx + c * lz};
    }
    StrataLineVertex lines[8];
    for (int k = 0; k < 4; ++k) {
        const Vec3 a = corners[k], b = corners[(k + 1) % 4];
        lines[2 * k] = {{a.x, a.y, a.z}, {0.95f, 0.75f, 0.35f, 0.9f}};
        lines[2 * k + 1] = {{b.x, b.y, b.z}, {0.95f, 0.75f, 0.35f, 0.9f}};
    }
    engine.submitLines(lines, 8);
}

void render(strata::Engine& engine, bridge::Scene& scene, const World& w, const Aim& aim, bool help,
            const std::vector<Message>& messages, double seconds) {
    scene.sync(w);
    scene.animate(w, seconds);
    std::vector<StrataPointLight> lights;
    scene.lights(w, seconds, setSky(engine, w), lights);
    engine.setPointLights(lights.data(), lights.size());
    int32_t width = 0, height = 0;
    engine.drawableSize(width, height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    engine.setCamera(playerCamera(w.player, aspect));
    StrataFrameDesc fd{};
    fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH | STRATA_CLEAR_SKY;
    engine.beginFrame(fd);
    drawHud(engine, w, aim, width, height, help, messages);
    drawPlacement(engine, w, aim);
    engine.endFrame();
}

Aim aimFor(const bridge::Scene& scene, const World& w) {
    const Vec3 eye{w.player.x, L::kEyeHeight, w.player.z};
    return scene.pick(w, eye, lookDirection(w.player.yaw, w.player.pitch), 12.0f);
}

// ---- the tour: a scripted visit for screenshots and CI smoke runs

struct TourStop {
    int ticks;          // ticks spent on this leg
    float forward;      // while walking
    float yaw, pitch;   // the view at the end of the leg
    Tool tool;          // held during the leg
    bool use;           // working with it the whole leg
    const char* name;   // screenshot file name, or nullptr for no shot
};

const TourStop kTour[] = {
    {1, 0, 0.0f, 0.05f, Tool::None, false, "01-yard"},
    {30, 1, 0.0f, -0.05f, Tool::None, false, "02-doorway"},
    {40, 1, -0.75f, -0.1f, Tool::None, false, "03-hearth"},
    {20, 0, -1.6f, 0.05f, Tool::None, false, "04-window"},
    {20, 0, -0.9f, 0.95f, Tool::None, false, "05-roof-hole"},
    // Sweep an arc of floor beside the player, then look at it.
    {20, 0, 1.6f, -0.95f, Tool::Broom, false, nullptr},
    {70, 0, 2.9f, -0.95f, Tool::Broom, true, nullptr},
    {15, 0, 2.2f, -0.7f, Tool::Broom, false, "06-swept-strip"},
    // Walk to the right-hand wall and scrub a stripe of it, then step back.
    {25, 1, 1.5708f, 0.0f, Tool::None, false, nullptr},
    {30, 0, 1.5708f, -0.35f, Tool::ScrubBrush, true, nullptr},
    {45, 0, 1.5708f, 0.45f, Tool::ScrubBrush, true, nullptr},
    {20, -1, 1.5708f, 0.05f, Tool::ScrubBrush, false, "07-scrubbed-wall"},
    // Across to the window, and pry two boards off it with the pry bar.
    {12, 0, -1.29f, 0.0f, Tool::PryBar, false, nullptr},
    {57, 1, -1.29f, 0.0f, Tool::PryBar, false, nullptr},
    {10, 0, -1.5708f, 0.18f, Tool::PryBar, false, nullptr},
    {68, 0, -1.5708f, 0.18f, Tool::PryBar, true, nullptr},
    {10, 0, -1.5708f, -0.25f, Tool::PryBar, false, nullptr},
    {68, 0, -1.5708f, -0.25f, Tool::PryBar, true, nullptr},
    {12, -1, -1.5708f, 0.05f, Tool::PryBar, false, "08-window-light"},
};

int runTour(strata::Engine& engine, const Args& args) {
    World w = newWorld(args.seed);
    bridge::Scene scene(engine);
    scene.sync(w);
    Aim aim;
    std::vector<Message> none;
    float yaw = w.player.yaw, pitch = 0;
    for (const TourStop& stop : kTour) {
        for (int t = 0; t < stop.ticks; ++t) {
            const float k = static_cast<float>(t + 1) / static_cast<float>(stop.ticks);
            Input in;
            in.forward = stop.forward;
            in.yaw = yaw + (stop.yaw - yaw) * k;
            in.pitch = pitch + (stop.pitch - pitch) * k;
            in.selectTool = static_cast<int32_t>(stop.tool);
            in.use = stop.use;
            aim = aimFor(scene, w);
            in.aim = aim;
            TickEvents ev;
            tick(w, in, &ev);
            scene.updateGrime(w, ev.grime);
            scene.sync(w);
        }
        yaw = stop.yaw;
        pitch = stop.pitch;
        if (!stop.name) continue;
        aim = aimFor(scene, w);
        render(engine, scene, w, aim, false, none, static_cast<double>(w.tick) / kTickRate);
        engine.present();
        const Description d = describe(w, aim);
        std::printf("%s: player (%.2f, %.2f) aim %s [kind %d index %d dist %.2f]\n", stop.name, w.player.x, w.player.z,
                    d.name.empty() ? "-" : d.name.c_str(), static_cast<int>(aim.kind), aim.index, aim.distance);
        if (!args.framesDir.empty()) engine.saveScreenshotPng(args.framesDir + "/" + stop.name + ".png");
    }
    const Progress p = progress(w);
    std::printf("step %s, floor %.1f%% clean, %zu meshes, fingerprint %016llx\n", stepName(p.step),
                p.floorClean * 100.0, scene.meshCount(), static_cast<unsigned long long>(fingerprint(w)));
    return 0;
}

// ---- the reveal: the restoration played back from its keyframes

constexpr double kRevealSeconds = 32.0;

class Reveal {
public:
    bool active() const { return active_; }
    void start(double now) {
        active_ = true;
        started_ = now;
        index_ = static_cast<size_t>(-1);
    }
    // Draws the reveal at `now`; false once it has finished (the scene is then
    // back on the real world).
    bool frame(strata::Engine& engine, bridge::Scene& scene, const World& w, double now) {
        if (!active_ || w.history.empty()) return false;
        const double t = (now - started_) / kRevealSeconds;
        if (t < 0) return false;  // not started yet
        if (t >= 1.0) {
            finish(scene, w);
            return false;
        }
        const size_t n = w.history.size();
        const size_t i = std::min(n - 1, static_cast<size_t>(t * static_cast<double>(n)));
        if (i != index_) {
            index_ = i;
            ghost_ = fromKeyframe(w, w.history[i]);
            scene.showPlayer(true);
            scene.sync(ghost_);
            scene.reloadGrime(ghost_);
        }
        int32_t width = 0, height = 0;
        engine.drawableSize(width, height);
        const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        // A slow drift across the room from the door's corner toward the hearth.
        const float k = static_cast<float>(t);
        const float eye[3] = {3.1f - 0.6f * k, 2.1f - 0.25f * k, 3.7f - 0.5f * k};
        const float target[3] = {-1.0f + 0.3f * k, 0.8f, -1.9f};
        const float up[3] = {0, 1, 0};
        engine.setCamera(strata::cameraLookAt(eye, target, up, 62.0f, aspect, 0.05f, 400.0f));
        scene.animate(ghost_, now);
        std::vector<StrataPointLight> lights;
        scene.lights(ghost_, now, lights);
        engine.setPointLights(lights.data(), lights.size());
        setSky(engine, ghost_);
        StrataFrameDesc fd{};
        fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH | STRATA_CLEAR_SKY;
        engine.beginFrame(fd);
        std::vector<StrataText> texts;
        std::vector<std::string> storage;
        storage.reserve(4);
        text(texts, 30, 30, 22, kEmber, "Ada's cabin, from ruin to home", storage);
        text(texts, 30, 62, 16, kWhite, clockText(ghost_) + "   " + stepName(progress(ghost_).step), storage);
        text(texts, 30, static_cast<float>(height) - 40, 14, kDim, "Esc to skip", storage);
        for (size_t j = 0; j < texts.size(); ++j) texts[j].utf8 = storage[j].c_str();
        engine.submitText(texts.data(), texts.size());
        engine.endFrame();
        return true;
    }
    void finish(bridge::Scene& scene, const World& w) {
        active_ = false;
        scene.showPlayer(false);
        scene.sync(w);
        scene.reloadGrime(w);
    }

private:
    bool active_ = false;
    double started_ = 0;
    size_t index_ = 0;
    World ghost_;
};

void writeReplayFile(const World& w, const std::string& path) {
    std::string error;
    if (writeReplay(w, path, &error)) std::printf("kindling: wrote the restoration replay to %s\n", path.c_str());
    else std::fprintf(stderr, "kindling: could not write %s: %s\n", path.c_str(), error.c_str());
}

// The autopilot plays the whole restoration headless, writing a first-person
// frame every N ticks, then the reveal's frames, then the replay.
int runHeadlessAutopilot(strata::Engine& engine, const Args& args) {
    World w = newWorld(args.seed);
    bridge::Scene scene(engine);
    scene.sync(w);
    Autopilot pilot;
    std::vector<Message> none;
    const int64_t limit = 60LL * 60 * kTickRate;
    int shot = 0;
    Step last = progress(w).step;
    while (progress(w).step != Step::Done && w.tick < limit) {
        const Input in = pilot.next(w);
        TickEvents ev;
        tick(w, in, &ev);
        scene.updateGrime(w, ev.grime);
        const Step now = progress(w).step;
        if (now != last) {
            std::printf("%7.1f s  %-8s %s\n", static_cast<double>(w.tick) / kTickRate, stepName(now), pilot.activity().c_str());
            last = now;
        }
        if (!args.framesDir.empty() && w.tick % args.frameEvery == 0) {
            const Aim aim = aimFor(scene, w);
            render(engine, scene, w, aim, false, none, static_cast<double>(w.tick) / kTickRate);
            char name[64];
            std::snprintf(name, sizeof name, "/frame-%05d.png", shot++);
            engine.saveScreenshotPng(args.framesDir + name);
        }
    }
    const bool done = progress(w).step == Step::Done;
    std::printf("%s after %.1f minutes of play; %zu keyframes; fingerprint %016llx\n",
                done ? "Done" : "NOT done", static_cast<double>(w.tick) / kTickRate / 60.0, w.history.size(),
                static_cast<unsigned long long>(fingerprint(w)));
    if (!args.revealFramesDir.empty()) {
        Reveal reveal;
        reveal.start(0);
        int frame = 0;
        for (double t = 0; reveal.frame(engine, scene, w, t); t += 1.0 / 12.0) {
            char name[64];
            std::snprintf(name, sizeof name, "/reveal-%04d.png", frame++);
            engine.saveScreenshotPng(args.revealFramesDir + name);
        }
        std::printf("wrote %d reveal frames\n", frame);
    }
    if (!args.replayPath.empty()) writeReplayFile(w, args.replayPath);
    return done ? 0 : 1;
}

int runGame(strata::Engine& engine, const Args& args) {
    World w = newWorld(args.seed);
    bridge::Scene scene(engine);
    scene.sync(w);
    Autopilot pilot;
    Reveal reveal;
    bool keys[kMaxScancode] = {};
    bool captured = false, help = true, running = true, mouseDown = false;
    bool interactPressed = false, lanternPressed = false;
    int32_t toolPressed = -1, rotatePressed = 0;
    float yaw = w.player.yaw, pitch = 0;
    Aim aim;
    std::vector<Message> messages;
    double clock = 0;
    bool revealed = false;

    auto capture = [&](bool on) {
        try {
            engine.setRelativeMouseMode(on);
            captured = on;
        } catch (const strata::Error&) {
            captured = false;
        }
    };
    capture(true);

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    double pending = 0;
    while (running) {
        StrataEvent e;
        while (engine.pollEvent(e)) {
            switch (e.type) {
                case STRATA_EVENT_QUIT: running = false; break;
                case STRATA_EVENT_KEY_DOWN:
                    if (e.scancode >= 0 && e.scancode < kMaxScancode) keys[e.scancode] = true;
                    if (e.repeat) break;
                    if (reveal.active()) {
                        if (e.scancode == kEscape) reveal.finish(scene, w);
                        break;
                    }
                    if (e.scancode == kEscape) {
                        if (captured) capture(false);
                        else running = false;
                    } else if (e.scancode == kF1) help = !help;
                    else if (e.scancode == kF12) engine.saveScreenshotPng("kindling-screenshot.png");
                    else if (e.scancode == kE) interactPressed = true;
                    else if (e.scancode == kR) rotatePressed += 1;
                    else if (e.scancode == kL) lanternPressed = true;
                    else if (e.scancode == kT && progress(w).step == Step::Done) reveal.start(clock);
                    else if (e.scancode == kGrave) toolPressed = 0;
                    else if (e.scancode >= k1 && e.scancode < k1 + kToolCount - 1) toolPressed = e.scancode - k1 + 1;
                    break;
                case STRATA_EVENT_KEY_UP:
                    if (e.scancode >= 0 && e.scancode < kMaxScancode) keys[e.scancode] = false;
                    break;
                case STRATA_EVENT_MOUSE_BUTTON_DOWN:
                    if (!captured) capture(true);
                    else if (e.button == 1) mouseDown = true;
                    break;
                case STRATA_EVENT_MOUSE_BUTTON_UP:
                    if (e.button == 1) mouseDown = false;
                    break;
                case STRATA_EVENT_MOUSE_MOTION:
                    if (captured) {
                        yaw += e.dx * kMouseSensitivity;
                        pitch = clampf(pitch - e.dy * kMouseSensitivity, -1.45f, 1.45f);
                    }
                    break;
                case STRATA_EVENT_RESIZED: engine.resize(e.width, e.height); break;
                default: break;
            }
        }

        const auto now = Clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (reveal.active()) {
            clock += dt;
            if (reveal.frame(engine, scene, w, clock)) {
                engine.present();
                continue;
            }
        }
        pending += dt;
        if (pending > 0.25) pending = 0.25;  // after a stall, do not try to catch up
        while (pending >= kTickSeconds) {
            pending -= kTickSeconds;
            clock += kTickSeconds;
            for (int k = 0; k < (args.autopilot ? args.speed : 1); ++k) {
                Input in;
                if (args.autopilot) {
                    in = pilot.next(w);
                } else {
                    in.forward = (keys[kW] ? 1.0f : 0.0f) - (keys[kS] ? 1.0f : 0.0f);
                    in.strafe = (keys[kD] ? 1.0f : 0.0f) - (keys[kA] ? 1.0f : 0.0f);
                    in.sprint = keys[kLShift] || keys[kRShift];
                    in.yaw = yaw;
                    in.pitch = pitch;
                    in.use = mouseDown && captured;
                    in.interact = interactPressed;
                    in.selectTool = toolPressed;
                    in.rotate = rotatePressed;
                    in.toggleLantern = lanternPressed;
                    aim = aimFor(scene, w);
                    in.aim = aim;
                }
                lanternPressed = interactPressed = false;
                toolPressed = -1;
                rotatePressed = 0;
                TickEvents ev;
                tick(w, in, &ev);
                if (args.autopilot) {
                    aim = in.aim;
                    yaw = w.player.yaw;
                    pitch = w.player.pitch;
                }
                scene.updateGrime(w, ev.grime);
                for (std::string& m : ev.messages) messages.push_back({std::move(m), clock + 6.0});
            }
            // Once Ada has settled, the reveal plays (once by itself; T plays it again).
            if (!revealed && progress(w).step == Step::Done) {
                revealed = true;
                writeReplayFile(w, args.replayPath);
                reveal.start(clock + 3.0);
            }
        }
        while (!messages.empty() && messages.front().until < clock) messages.erase(messages.begin());
        if (messages.size() > 3) messages.erase(messages.begin(), messages.end() - 3);
        render(engine, scene, w, aim, help, messages, clock);
        engine.present();
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parseArgs(argc, argv, args)) {
        usage();
        return 2;
    }
    try {
        strata::EngineDesc desc;
        desc.surface = args.headless ? STRATA_SURFACE_HEADLESS : STRATA_SURFACE_WINDOW;
        desc.width = args.width;
        desc.height = args.height;
        desc.title = "Kindling";
        auto engine = strata::Engine::create(desc);
        setLighting(*engine);
        if (args.tour) return runTour(*engine, args);
        if (args.headless) return runHeadlessAutopilot(*engine, args);
        return runGame(*engine, args);
    } catch (const strata::Error& e) {
        std::fprintf(stderr, "kindling: %s\n", e.what());
        return 1;
    }
}
