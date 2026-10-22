// kindling: the game on the Strata engine.
//
//   kindling [--slot DIR] [--new] [--seed N] [--width W --height H] [--autopilot [--speed N]] [--replay PATH]
//            [--playtest-out DIR]
//   kindling --headless --tour --frames DIR [--width W --height H]
//   kindling --headless --autopilot [--frames DIR --frame-every N] [--reveal-frames DIR] [--replay PATH]
//   kindling --headless --closeups DIR
//            [--playtest-out DIR]
//
// --playtest-out DIR writes state.json and events.jsonl for AI playtesters
// (src/playtest/probe.h).
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
#include <memory>
#include <iterator>
#include <string>
#include <vector>

#include "bridge/scene.h"
#include "play/hud.h"
#include "play/session.h"
#include "playtest/probe.h"
#include "save/save.h"
#include "sim/autopilot.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/rules.h"
#include "strata/strata.hpp"

using namespace kindling;
using play::aimFor;
using play::Help;
using play::Message;
using play::render;

namespace {

struct Args {
    uint64_t seed = 1881;
    bool headless = false;
    bool tour = false;
    bool autopilot = false;
    int speed = 1;             // autopilot ticks per frame in a window
    std::string framesDir;
    int frameEvery = 300;      // headless autopilot: ticks between frames
    std::string revealFramesDir;
    std::string closeupsDir;   // headless: close-ups of the people, the mare and the furniture
    std::string replayPath;    // default: in the save slot
    std::string slot;          // default: defaultSlot()
    bool fresh = false;        // start a new game even if there is a save
    std::string playtestOut;   // --playtest-out: the probe's directory
    int width = 1280, height = 720;
};

int64_t wallClockMs() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

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
        else if (s == "--closeups") a.closeupsDir = next("--closeups");
        else if (s == "--replay") a.replayPath = next("--replay");
        else if (s == "--slot") a.slot = next("--slot");
        else if (s == "--new") a.fresh = true;
        else if (s == "--playtest-out") a.playtestOut = next("--playtest-out");
        else if (s == "--width") a.width = std::atoi(next("--width"));
        else if (s == "--height") a.height = std::atoi(next("--height"));
        else if (s == "--help" || s == "-h") return false;
        else {
            std::fprintf(stderr, "kindling: unknown argument '%s'\n", s.c_str());
            return false;
        }
    }
    if (a.headless && !a.tour && !a.autopilot && a.closeupsDir.empty()) {
        std::fprintf(stderr, "kindling: --headless needs --tour, --autopilot or --closeups (there is no one to play)\n");
        return false;
    }
    return true;
}

void usage() {
    std::puts("usage: kindling [--slot DIR] [--new] [--seed N] [--width W --height H] [--autopilot [--speed N]]\n"
              "                [--replay PATH] [--playtest-out DIR]\n"
              "       kindling --headless --tour --frames DIR [--width W --height H]\n"
              "       kindling --headless --autopilot [--frames DIR --frame-every N] [--reveal-frames DIR]\n"
              "                [--replay PATH] [--playtest-out DIR]");
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
        render(engine, scene, w, aim, Help::Hidden, none, static_cast<double>(w.tick) / kTickRate);
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
    std::unique_ptr<playtest::Recorder> recorder;
    if (!args.playtestOut.empty()) {
        recorder = std::make_unique<playtest::Recorder>(args.playtestOut, wallClockMs);
        recorder->worldStarted(w, "created");
    }
    while (progress(w).step != Step::Done && w.tick < limit) {
        const Input in = pilot.next(w);
        TickEvents ev;
        tick(w, in, &ev);
        scene.updateGrime(w, ev.grime);
        if (recorder) {
            playtest::HostView view;
            view.clock = play::clockText(w);
            for (const play::HudRow& row : play::checklist(w)) view.checklist.emplace_back(row.done, row.text);
            recorder->update(w, view, ev.messages, 0);
        }
        const Step now = progress(w).step;
        if (now != last) {
            std::printf("%7.1f s  %-8s %s\n", static_cast<double>(w.tick) / kTickRate, stepName(now), pilot.activity().c_str());
            last = now;
        }
        if (!args.framesDir.empty() && w.tick % args.frameEvery == 0) {
            const Aim aim = aimFor(scene, w);
            render(engine, scene, w, aim, Help::Hidden, none, static_cast<double>(w.tick) / kTickRate);
            char name[64];
            std::snprintf(name, sizeof name, "/frame-%05d.png", shot++);
            engine.saveScreenshotPng(args.framesDir + name);
        }
    }
    const bool done = progress(w).step == Step::Done;
    // Then the mare: fed and brushed, with a last frame of her.
    for (int i = 0; done && i < 120 * kTickRate && !(w.horse.dirt <= 0 && w.horse.hunger < 0.1f); ++i) {
        TickEvents ev;
        tick(w, pilot.next(w), &ev);
        scene.updateGrime(w, ev.grime);
    }
    for (int i = 0; done && i < 3 * kTickRate; ++i) tick(w, pilot.next(w));
    if (done && !args.framesDir.empty()) {
        render(engine, scene, w, aimFor(scene, w), Help::Hidden, none, static_cast<double>(w.tick) / kTickRate);
        engine.saveScreenshotPng(args.framesDir + "/frame-mare.png");
    }
    std::printf("%s after %.1f minutes of play; %zu keyframes; fingerprint %016llx\n",
                done ? "Done" : "NOT done", static_cast<double>(w.tick) / kTickRate / 60.0, w.history.size(),
                static_cast<unsigned long long>(fingerprint(w)));
    if (!args.revealFramesDir.empty()) {
        play::Reveal reveal;
        reveal.start(0);
        int frame = 0;
        for (double t = 0; reveal.frame(engine, scene, w, t); t += 1.0 / 12.0) {
            char name[64];
            std::snprintf(name, sizeof name, "/reveal-%04d.png", frame++);
            engine.saveScreenshotPng(args.revealFramesDir + name);
        }
        std::printf("wrote %d reveal frames\n", frame);
    }
    if (!args.replayPath.empty()) play::writeReplayFile(w, args.replayPath);
    return done ? 0 : 1;
}

// ---- close-ups: the models seen near, by daylight, for checking art

// Puts the player's eye at (x, z) looking at `target`, so a first-person frame
// is a close-up of it.
void lookAt(World& w, float x, float z, Vec3 target) {
    w.player.x = x;
    w.player.z = z;
    const float dx = target.x - x, dz = target.z - z, dy = target.y - layout::kEyeHeight;
    w.player.yaw = detAtan2(dx, -dz);
    w.player.pitch = detAtan2(dy, std::sqrt(dx * dx + dz * dz));
}

// The tick nearest mid-afternoon on the world's own day, so close-ups are lit
// by daylight.
int64_t afternoonTick(const World& w) {
    World probe = w;
    probe.tick = 0;
    while (timeOfDay(probe) < 14.5 / 24.0) probe.tick += kTickRate;
    return probe.tick;
}

int runCloseups(strata::Engine& engine, const Args& args) {
    struct Shot {
        const char* name;
        float x, z;
        Vec3 target;
    };
    auto shoot = [&](World w, const Shot* shots, size_t n) {
        bridge::Scene scene(engine);
        w.tick = afternoonTick(w);
        w.lantern = false;
        std::vector<Message> none;
        for (size_t i = 0; i < n; ++i) {
            lookAt(w, shots[i].x, shots[i].z, shots[i].target);
            render(engine, scene, w, Aim{}, Help::Hidden, none, 10.0, true);  // no words
            engine.present();
            engine.saveScreenshotPng(args.closeupsDir + "/" + shots[i].name + ".png");
            std::printf("%s\n", shots[i].name);
        }
    };
    // The ruin as found.
    const Shot ruin[] = {
        {"chair-broken", 1.0f, 0.3f, {1.65f, 0.6f, -1.15f}},
        {"table-toppled", -1.9f, 2.6f, {-3.3f, 0.3f, 1.6f}},
    };
    shoot(newWorld(args.seed), ruin, std::size(ruin));
    // The cabin restored: the autopilot plays it through, the mare included.
    World w = newWorld(args.seed);
    Autopilot pilot;
    for (int i = 0; i < 60 * 60 * kTickRate && progress(w).step != Step::Done; ++i) tick(w, pilot.next(w));
    for (int i = 0; i < 120 * kTickRate && !(w.horse.dirt <= 0 && w.horse.hunger < 0.1f); ++i) tick(w, pilot.next(w));
    const Vec3 ada{w.settler.x, 1.0f, w.settler.z};
    const Shot home[] = {
        {"ada", w.settler.x + 0.9f, w.settler.z - 1.4f, ada},
        {"chair-mended", 0.6f, 0.0f, {1.65f, 0.7f, -1.15f}},
        {"table", -1.9f, 2.6f, {-3.5f, 0.6f, 1.45f}},
        {"mare-side", layout::kHorse.x - 2.4f, layout::kHorse.z + 0.1f, {layout::kHorse.x, 1.0f, layout::kHorse.z}},
        {"mare-head", layout::kHorse.x - 0.9f, layout::kHorse.z - 2.1f, {layout::kHorse.x, 1.7f, layout::kHorse.z - 1.3f}},
    };
    shoot(w, home, std::size(home));
    return 0;
}

int runGame(strata::Engine& engine, const Args& args) {
    play::SessionOptions o;
    o.seed = args.seed;
    o.slot = args.slot.empty() ? defaultSlot() : args.slot;
    o.replayPath = args.replayPath.empty() ? o.slot + "/kindling-restoration.usda" : args.replayPath;
    o.fresh = args.fresh;
    o.autopilot = args.autopilot;
    o.speed = args.speed;
    o.playtestOut = args.playtestOut;
    play::Session session(engine, o);
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    while (session.running()) {
        StrataEvent e;
        while (engine.pollEvent(e)) session.handle(e);
        const auto now = Clock::now();
        session.frame(std::chrono::duration<double>(now - last).count());
        last = now;
        engine.present();
    }
    session.save(nullptr);
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
        play::setLighting(*engine);
        if (args.tour) return runTour(*engine, args);
        if (!args.closeupsDir.empty()) return runCloseups(*engine, args);
        if (args.headless) return runHeadlessAutopilot(*engine, args);
        return runGame(*engine, args);
    } catch (const strata::Error& e) {
        std::fprintf(stderr, "kindling: %s\n", e.what());
        return 1;
    }
}
