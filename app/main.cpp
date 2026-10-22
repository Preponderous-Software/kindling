// kindling: the game on the Strata engine.
//
//   kindling [--seed N] [--width W --height H]
//   kindling --headless --tour --frames DIR [--width W --height H]
//
// Controls: mouse to look, W A S D to walk, Shift to hurry, 1-4 to take a tool
// (` for bare hands), hold the left button to work, E to pick up or put down,
// F1 for help, Esc to free the mouse (Esc again quits). --tour plays a short
// scripted visit and, headless, writes a screenshot at each stop to DIR.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bridge/scene.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/rules.h"
#include "strata/strata.hpp"

using namespace kindling;
namespace L = kindling::layout;

namespace {

// SDL3 scancodes (USB HID usage ids), which Strata reports as-is.
constexpr int kA = 4, kD = 7, kE = 8, kS = 22, kW = 26, k1 = 30, kEscape = 41, kGrave = 53, kF1 = 58, kF12 = 69,
              kLShift = 225, kRShift = 229;
constexpr int kMaxScancode = 512;
constexpr float kMouseSensitivity = 0.0025f;  // radians per pixel
constexpr float kFovY = 70.0f;

struct Args {
    uint64_t seed = 1881;
    bool headless = false;
    bool tour = false;
    std::string framesDir;
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
        else if (s == "--width") a.width = std::atoi(next("--width"));
        else if (s == "--height") a.height = std::atoi(next("--height"));
        else if (s == "--help" || s == "-h") return false;
        else {
            std::fprintf(stderr, "kindling: unknown argument '%s'\n", s.c_str());
            return false;
        }
    }
    if (a.headless && !a.tour) {
        std::fprintf(stderr, "kindling: --headless needs --tour (there is no one to play)\n");
        return false;
    }
    return true;
}

void usage() {
    std::puts("usage: kindling [--seed N] [--width W --height H]\n"
              "       kindling --headless --tour --frames DIR [--width W --height H]");
}

// Late morning on 11 November, in a Montana valley: the sun low in the south.
void setDaylight(strata::Engine& engine) {
    StrataLighting light = strata::lightingDefaults(STRATA_LIGHTING_SUN);
    light.cascade_count = 4;
    light.shadow_map_size = 2048;
    light.shadow_distance = 45.0f;
    light.fog = 1;
    engine.setLighting(light);
    const double unixSeconds = 1794418200.0;  // 2026-11-11 10:30 at UTC-7; the sky's geometry, not the year
    const StrataSolarPosition sun = strata::solarPosition(unixSeconds, 46.6, -112.0);
    engine.setSunDirection(sun.to_sun);
    // Daylight spilling in through the door and the roof hole: a soft fill that
    // stands in for bounced light, which the renderer does not compute.
    const StrataPointLight fill{{1.5f, 2.2f, 2.6f}, 8.0f, {0.75f, 0.82f, 1.0f}, 0.5f, 0};
    const StrataPointLight shaft{{0.1f, 2.6f, -0.9f}, 4.5f, {0.75f, 0.82f, 1.0f}, 0.6f, 0};
    const StrataPointLight lights[2] = {fill, shaft};
    engine.setPointLights(lights, 2);
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
        text(texts, cx - 4.5f * static_cast<float>(d.name.size()), cy + 28, 18, c, d.name, storage);
        if (!d.hint.empty())
            text(texts, cx - 4.0f * static_cast<float>(d.hint.size()), cy + 52, 16, kDim, d.hint, storage);
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
    } else {
        text(texts, 20, y, 16, kDim, "Cleared. Repairs come in the next milestone.", storage);
    }

    // The tool belt, and what is in hand.
    std::string belt;
    for (int t = 1; t < kToolCount; ++t) {
        const bool held = static_cast<int>(w.tool) == t;
        belt += (held ? "[" : " ") + std::to_string(t) + " " + toolName(static_cast<Tool>(t)) + (held ? "]" : " ") + "  ";
    }
    text(texts, cx - 4.0f * static_cast<float>(belt.size()), static_cast<float>(height) - 40, 16, kWhite, belt, storage);
    if (w.carry == Carry::Debris)
        text(texts, cx - 120, static_cast<float>(height) - 68, 16, kEmber, "Carrying: E to put down (outside to haul out)",
             storage);

    float my = cy + 90;
    for (const Message& m : messages) {
        text(texts, cx - 4.0f * static_cast<float>(m.text.size()), my, 16, kEmber, m.text, storage);
        my += 22;
    }

    if (help) {
        const char* lines[] = {"Mouse      look",       "W A S D    walk",          "Shift      hurry",
                               "1-4        take a tool", "Left mouse work with it", "E          pick up / put down",
                               "Esc        free the mouse / quit", "F1  hide help   F12  screenshot"};
        float hy = static_cast<float>(height) - 24 * 8 - 80;
        for (const char* l : lines) {
            text(texts, 20, hy, 16, kDim, l, storage);
            hy += 24;
        }
    }
    for (size_t i = 0; i < texts.size(); ++i) texts[i].utf8 = storage[i].c_str();
    engine.submitText(texts.data(), texts.size());
}

void render(strata::Engine& engine, bridge::Scene& scene, const World& w, const Aim& aim, bool help,
            const std::vector<Message>& messages) {
    scene.sync(w);
    int32_t width = 0, height = 0;
    engine.drawableSize(width, height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    engine.setCamera(playerCamera(w.player, aspect));
    StrataFrameDesc fd{};
    fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH | STRATA_CLEAR_SKY;
    engine.beginFrame(fd);
    drawHud(engine, w, aim, width, height, help, messages);
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
        render(engine, scene, w, aim, false, none);
        engine.present();
        const Description d = describe(w, aim);
        std::printf("%s: player (%.2f, %.2f) aim %s\n", stop.name, w.player.x, w.player.z,
                    d.name.empty() ? "-" : d.name.c_str());
        if (!args.framesDir.empty()) engine.saveScreenshotPng(args.framesDir + "/" + stop.name + ".png");
    }
    const Progress p = progress(w);
    std::printf("step %s, floor %.1f%% clean, %zu meshes, fingerprint %016llx\n", stepName(p.step),
                p.floorClean * 100.0, scene.meshCount(), static_cast<unsigned long long>(fingerprint(w)));
    return 0;
}

int runGame(strata::Engine& engine, const Args& args) {
    World w = newWorld(args.seed);
    bridge::Scene scene(engine);
    scene.sync(w);
    bool keys[kMaxScancode] = {};
    bool captured = false, help = true, running = true, mouseDown = false;
    bool interactPressed = false;
    int32_t toolPressed = -1;
    float yaw = w.player.yaw, pitch = 0;
    Aim aim;
    std::vector<Message> messages;
    double clock = 0;

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
                    if (e.scancode == kEscape) {
                        if (captured) capture(false);
                        else running = false;
                    } else if (e.scancode == kF1) help = !help;
                    else if (e.scancode == kF12) engine.saveScreenshotPng("kindling-screenshot.png");
                    else if (e.scancode == kE) interactPressed = true;
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
        pending += std::chrono::duration<double>(now - last).count();
        last = now;
        if (pending > 0.25) pending = 0.25;  // after a stall, do not try to catch up
        while (pending >= kTickSeconds) {
            pending -= kTickSeconds;
            clock += kTickSeconds;
            Input in;
            in.forward = (keys[kW] ? 1.0f : 0.0f) - (keys[kS] ? 1.0f : 0.0f);
            in.strafe = (keys[kD] ? 1.0f : 0.0f) - (keys[kA] ? 1.0f : 0.0f);
            in.sprint = keys[kLShift] || keys[kRShift];
            in.yaw = yaw;
            in.pitch = pitch;
            in.use = mouseDown && captured;
            in.interact = interactPressed;
            in.selectTool = toolPressed;
            interactPressed = false;
            toolPressed = -1;
            aim = aimFor(scene, w);
            in.aim = aim;
            TickEvents ev;
            tick(w, in, &ev);
            scene.updateGrime(w, ev.grime);
            for (std::string& m : ev.messages) messages.push_back({std::move(m), clock + 6.0});
        }
        while (!messages.empty() && messages.front().until < clock) messages.erase(messages.begin());
        if (messages.size() > 3) messages.erase(messages.begin(), messages.end() - 3);
        render(engine, scene, w, aim, help, messages);
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
        setDaylight(*engine);
        return args.tour ? runTour(*engine, args) : runGame(*engine, args);
    } catch (const strata::Error& e) {
        std::fprintf(stderr, "kindling: %s\n", e.what());
        return 1;
    }
}
