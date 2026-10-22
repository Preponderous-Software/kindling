// The playtest probe (--playtest-out): headings and bearings point the right
// way, state.json carries the probe contract's core fields, the recorder
// writes one objective_completed event per restoration step as the autopilot
// plays, and recording never changes the world.
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "playtest/probe.h"
#include "sim/autopilot.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/rules.h"

using namespace kindling;
namespace fs = std::filesystem;

namespace {

bool approx(float a, float b, float eps = 0.5f) { return std::fabs(a - b) <= eps; }

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::vector<std::string> lines(const std::string& path) {
    std::ifstream f(path);
    std::vector<std::string> out;
    for (std::string line; std::getline(f, line);)
        if (!line.empty()) out.push_back(line);
    return out;
}

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

}  // namespace

int main(int argc, char** argv) {
    // ---- headings and bearings: yaw 0 faces -z, and turning right grows both
    CHECK(approx(playtest::headingDegrees(0), 0));
    CHECK(approx(playtest::headingDegrees(1.5707963f), 90));
    CHECK(approx(playtest::headingDegrees(-1.5707963f), 270));
    Player p;  // at the origin, facing -z
    CHECK(approx(playtest::bearingTo(p, 0, -5), 0));
    CHECK(approx(playtest::bearingTo(p, 5, 0), 90));    // +x is to the right
    CHECK(approx(playtest::bearingTo(p, -5, 0), -90));
    CHECK(approx(std::fabs(playtest::bearingTo(p, 0, 5)), 180));
    p.yaw = 1.5707963f;  // turned right a quarter: now facing +x
    CHECK(approx(playtest::bearingTo(p, 5, 0), 0));
    CHECK(approx(playtest::bearingTo(p, 0, 5), 90));

    // ---- strings are escaped
    CHECK(playtest::quote("a \"b\"\n\\") == "\"a \\\"b\\\"\\n\\\\\"");

    // ---- state.json: the contract's core fields, and the world's
    World w = newWorld(1881);
    playtest::HostView view;
    view.clock = "9:00 am";
    view.checklist.emplace_back(false, "Debris");
    const std::string state = playtest::stateJson(w, view, 1700000000000LL, 30.0);
    for (const char* field : {"\"wallTimeMs\":1700000000000", "\"version\":\"", "\"screen\":\"WORLD\"", "\"world\":true",
                              "\"seed\":1881", "\"tick\":0", "\"player\":{\"x\":2.5,\"z\":6.2,\"yawDeg\":0",
                              "\"objective\":{\"number\":1,\"title\":\"Survey\"", "\"nearest\":{\"door\":[",
                              "\"debris\":[{\"name\":", "\"checklist\":[{\"done\":false,\"text\":\"Debris\"}]"})
        CHECK(has(state, field));
    // From the spawn the door is straight ahead, a couple of metres off.
    const std::vector<playtest::Target> targets = playtest::nearest(w);
    bool sawDoor = false;
    for (const playtest::Target& t : targets)
        if (t.kind == "door") {
            sawDoor = true;
            CHECK(approx(t.bearing, 0, 1));
            CHECK(t.distance > 1 && t.distance < 3);
        }
    CHECK(sawDoor);

    // ---- the recorder, over a whole autopilot restoration
    const fs::path dir = fs::path(argc > 1 ? argv[1] : ".") / "playtest-probe-test";
    fs::remove_all(dir);
    fs::create_directories(dir);
    int64_t nowMs = 1700000000000LL;
    playtest::Recorder recorder(dir.string(), [&nowMs] { return nowMs; });
    World played = newWorld(1881);
    World unrecorded = newWorld(1881);
    Autopilot pilot, pilot2;
    recorder.worldStarted(played, "created");
    const int64_t limit = 60LL * 60 * kTickRate;
    while (progress(played).step != Step::Done && played.tick < limit) {
        TickEvents ev;
        tick(played, pilot.next(played), &ev);
        tick(unrecorded, pilot2.next(unrecorded));
        nowMs += 33;
        recorder.update(played, view, ev.messages, played.tick > 600 ? 1 : 0);
    }
    CHECK(progress(played).step == Step::Done);
    CHECK(fingerprint(played) == fingerprint(unrecorded));  // recording changes nothing

    const std::vector<std::string> events = lines((dir / "events.jsonl").string());
    CHECK(!events.empty());
    CHECK(has(events.at(0), "{\"type\":\"world\",\"tick\":0,\"wallTimeMs\":1700000000000,\"how\":\"created\",\"seed\":1881}"));
    CHECK(has(events.at(1), "\"type\":\"screen\"") && has(events.at(1), "\"from\":null,\"to\":\"WORLD\""));
    int objectives = 0, saves = 0, statuses = 0;
    int64_t lastTick = 0;
    for (const std::string& e : events) {
        if (has(e, "\"type\":\"objective_completed\"")) ++objectives;
        if (has(e, "\"type\":\"save\"")) ++saves;
        if (has(e, "\"type\":\"status\",") && has(e, "\"text\":\"")) ++statuses;
        const size_t at = e.find("\"tick\":");
        const int64_t t = std::stoll(e.substr(at + 7));
        CHECK(t >= lastTick);  // ticks never go backwards within one world
        lastTick = t;
    }
    CHECK(objectives == 6);  // Survey, Clear, Repair, Furnish, Kindle, Welcome
    CHECK(saves == 1);
    CHECK(statuses > 0);
    CHECK(has(slurp((dir / "events.jsonl").string()),
              "\"type\":\"objective_completed\",\"tick\":"));
    CHECK(has(slurp((dir / "events.jsonl").string()), "\"number\":1,\"id\":\"Survey\",\"title\":\"Survey\""));
    const std::string last = slurp((dir / "state.json").string());
    CHECK(has(last, "\"world\":true") && has(last, "\"tps\":"));
    CHECK(!fs::exists(dir / "state.json.tmp"));
    fs::remove_all(dir);

    TEST_MAIN_END();
}
