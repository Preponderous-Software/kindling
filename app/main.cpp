// kindling: the game on the Strata engine.
//
//   kindling [--seed N] [--width W --height H]
//   kindling --headless --tour --frames DIR [--width W --height H]
//
// Controls: mouse to look, W A S D to walk, Shift to hurry, F1 for help, Esc
// to free the mouse (Esc again quits). --tour plays a short scripted walk into
// the cabin and, headless, writes a screenshot at each stop to DIR.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "bridge/scene.h"
#include "sim/layout.h"
#include "sim/rules.h"
#include "strata/strata.hpp"

using namespace kindling;
namespace L = kindling::layout;

namespace {

// SDL3 scancodes (USB HID usage ids), which Strata reports as-is.
constexpr int kA = 4, kD = 7, kS = 22, kW = 26, kEscape = 41, kF1 = 58, kF12 = 69, kLShift = 225, kRShift = 229;
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

void drawHud(strata::Engine& engine, const World& w, const Aim& aim, int width, int height, bool help) {
    std::vector<StrataText> texts;
    std::vector<std::string> storage;
    storage.reserve(32);
    const float cx = width * 0.5f, cy = height * 0.5f;
    text(texts, cx - 6, cy - 8, 16, kWhite, "+", storage);

    const Description d = describe(w, aim);
    if (!d.name.empty() && aim.distance <= L::kReach * 2.5f) {
        text(texts, cx - 4.5f * static_cast<float>(d.name.size()), cy + 28, 18, kWhite, d.name, storage);
        if (!d.hint.empty())
            text(texts, cx - 4.0f * static_cast<float>(d.hint.size()), cy + 52, 16, kDim, d.hint, storage);
    }

    // The survey checklist, until every kind of damage has been found.
    if ((w.surveyed & kSurveyAll) != kSurveyAll) {
        struct Row {
            uint32_t bit;
            const char* label;
        };
        const Row rows[] = {{kSurveyGrime, "Grime"},
                            {kSurveyDebris, "Debris"},
                            {kSurveyRottenPlank, "Rotten floorboards"},
                            {kSurveyRoofHole, "Hole in the roof"},
                            {kSurveyBoardedWindow, "Boarded window"}};
        float y = 20;
        text(texts, 20, y, 18, kEmber, "SURVEY THE CABIN", storage);
        y += 30;
        for (const Row& r : rows) {
            const bool found = (w.surveyed & r.bit) != 0;
            text(texts, 20, y, 16, found ? kWhite : kDim, std::string(found ? "[x] " : "[ ] ") + r.label, storage);
            y += 24;
        }
    } else {
        text(texts, 20, 20, 16, kDim, "Survey done. The real work starts in the next milestone.", storage);
    }

    if (help) {
        const char* lines[] = {"Mouse      look", "W A S D    walk", "Shift      hurry", "Esc        free the mouse / quit",
                               "F1         hide this help", "F12        screenshot"};
        float y = static_cast<float>(height) - 24 * 6 - 20;
        for (const char* l : lines) {
            text(texts, 20, y, 16, kDim, l, storage);
            y += 24;
        }
    }
    for (size_t i = 0; i < texts.size(); ++i) texts[i].utf8 = storage[i].c_str();
    engine.submitText(texts.data(), texts.size());
}

void render(strata::Engine& engine, bridge::Scene& scene, const World& w, const Aim& aim, bool help) {
    scene.sync(w);
    int32_t width = 0, height = 0;
    engine.drawableSize(width, height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    engine.setCamera(playerCamera(w.player, aspect));
    StrataFrameDesc fd{};
    fd.clear_flags = STRATA_CLEAR_COLOR | STRATA_CLEAR_DEPTH | STRATA_CLEAR_SKY;
    engine.beginFrame(fd);
    drawHud(engine, w, aim, width, height, help);
    engine.endFrame();
}

unsigned surveyedCount(uint32_t bits) {
    unsigned n = 0;
    for (; bits; bits &= bits - 1) ++n;
    return n;
}

Aim aimFor(const bridge::Scene& scene, const World& w) {
    const Vec3 eye{w.player.x, L::kEyeHeight, w.player.z};
    return scene.pick(w, eye, lookDirection(w.player.yaw, w.player.pitch), 12.0f);
}

// ---- the tour: a scripted walk for screenshots and CI smoke runs

struct TourStop {
    int ticks;         // ticks spent getting here
    float forward;     // while walking
    float yaw, pitch;  // the view at the stop
    const char* name;  // screenshot file name
};

const TourStop kTour[] = {
    {1, 0, 0.0f, 0.05f, "01-yard"},
    {95, 1, 0.0f, -0.05f, "02-doorway"},
    {40, 1, -0.35f, -0.12f, "03-hearth"},
    {20, 0, -1.25f, 0.05f, "04-window"},
    {20, 0, 0.3f, 0.95f, "05-roof-hole"},
    {20, 0, 0.75f, -0.45f, "06-floor"},
};

int runTour(strata::Engine& engine, const Args& args) {
    World w = newWorld(args.seed);
    bridge::Scene scene(engine);
    scene.sync(w);
    Aim aim;
    float yaw = w.player.yaw, pitch = 0;
    for (const TourStop& stop : kTour) {
        for (int t = 0; t < stop.ticks; ++t) {
            const float k = static_cast<float>(t + 1) / static_cast<float>(stop.ticks);
            Input in;
            in.forward = stop.forward;
            in.yaw = yaw + (stop.yaw - yaw) * k;
            in.pitch = pitch + (stop.pitch - pitch) * k;
            aim = aimFor(scene, w);
            in.aim = aim;
            tick(w, in);
        }
        yaw = stop.yaw;
        pitch = stop.pitch;
        aim = aimFor(scene, w);
        render(engine, scene, w, aim, false);
        engine.present();
        const Description d = describe(w, aim);
        std::printf("%s: player (%.2f, %.2f) aim %s\n", stop.name, w.player.x, w.player.z,
                    d.name.empty() ? "-" : d.name.c_str());
        if (!args.framesDir.empty()) engine.saveScreenshotPng(args.framesDir + "/" + stop.name + ".png");
    }
    std::printf("surveyed %u of %u kinds, %zu meshes, fingerprint %016llx\n",
                surveyedCount(w.surveyed), 5u, scene.meshCount(),
                static_cast<unsigned long long>(fingerprint(w)));
    return 0;
}

int runGame(strata::Engine& engine, const Args& args) {
    World w = newWorld(args.seed);
    bridge::Scene scene(engine);
    scene.sync(w);
    bool keys[kMaxScancode] = {};
    bool captured = false, help = true, running = true;
    float yaw = w.player.yaw, pitch = 0;
    Aim aim;

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
                    break;
                case STRATA_EVENT_KEY_UP:
                    if (e.scancode >= 0 && e.scancode < kMaxScancode) keys[e.scancode] = false;
                    break;
                case STRATA_EVENT_MOUSE_BUTTON_DOWN:
                    if (!captured) capture(true);
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
            Input in;
            in.forward = (keys[kW] ? 1.0f : 0.0f) - (keys[kS] ? 1.0f : 0.0f);
            in.strafe = (keys[kD] ? 1.0f : 0.0f) - (keys[kA] ? 1.0f : 0.0f);
            in.sprint = keys[kLShift] || keys[kRShift];
            in.yaw = yaw;
            in.pitch = pitch;
            aim = aimFor(scene, w);
            in.aim = aim;
            tick(w, in);
        }
        render(engine, scene, w, aim, help);
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
