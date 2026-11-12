// Save play-on fingerprints (Preponderous RFC 0036). Every frozen save in
// tests/fixtures/ is loaded, checked against the fingerprint it had when it
// was written, played on for a fixed number of ticks by a fixed driver, and
// checked against the fingerprint committed in tests/fixtures/playon.expect.
//
// The two columns mean different things:
//   load      the world exactly as the released build saved it. It never changes:
//             a mismatch means a player's save no longer loads as what they saved.
//   play-on   that world after N more ticks under today's rules. It changes when
//             the rules change on purpose, and then the line is re-pinned in the
//             same pull request with the reason; otherwise a mismatch is a break.
//
// No engine and no clock: decode, tick, hash with the game's own fingerprint()
// (src/sim/world.cpp), not Strata's StateHasher.
#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "save/save.h"
#include "sim/autopilot.h"

using namespace kindling;
namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

struct Line {
    std::string fixture, driver;
    int64_t ticks = 0;
    uint64_t load = 0, playOn = 0;
};

std::vector<Line> readExpect(const std::string& path) {
    std::vector<Line> lines;
    std::ifstream f(path);
    std::string text;
    while (std::getline(f, text)) {
        if (text.empty() || text[0] == '#') continue;
        std::istringstream in(text);
        Line l;
        std::string load, playOn;
        in >> l.fixture >> l.driver >> l.ticks >> load >> playOn;
        l.load = std::stoull(load, nullptr, 16);
        l.playOn = std::stoull(playOn, nullptr, 16);
        lines.push_back(l);
    }
    return lines;
}

// The world after `ticks` more ticks from a loaded save. `autopilot` plays the
// game from where the save left it; `idle` gives no input (time still passes).
bool playOn(World& w, const std::string& driver, int64_t ticks) {
    if (driver == "autopilot") {
        Autopilot pilot;
        for (int64_t i = 0; i < ticks; ++i) tick(w, pilot.next(w));
        return true;
    }
    if (driver == "idle") {
        for (int64_t i = 0; i < ticks; ++i) tick(w, Input{});
        return true;
    }
    return false;
}

}  // namespace

int main() {
    const std::string dir = KINDLING_FIXTURES;
    const std::vector<Line> lines = readExpect(dir + "/playon.expect");
    CHECK(!lines.empty());

    // Every frozen save is covered: a new fixture without a line fails here.
    std::set<std::string> listed;
    for (const Line& l : lines) listed.insert(l.fixture);
    for (const auto& e : fs::directory_iterator(dir))
        if (e.path().extension() == ".save") {
            const bool covered = listed.count(e.path().filename().string()) == 1;
            CHECK(covered);
            if (!covered) std::fprintf(stderr, "no playon.expect line for %s\n", e.path().filename().string().c_str());
        }

    for (const Line& l : lines) {
        const std::vector<uint8_t> bytes = readAll(dir + "/" + l.fixture);
        World w;
        std::string why;
        const bool ok = decodeSave(bytes, w, &why);
        CHECK(ok);
        if (!ok) {
            std::fprintf(stderr, "%s refused: %s\n", l.fixture.c_str(), why.c_str());
            continue;
        }
        const uint64_t load = fingerprint(w);
        CHECK(load == l.load);

        World again;
        CHECK(decodeSave(bytes, again, nullptr));
        CHECK(playOn(w, l.driver, l.ticks));
        playOn(again, l.driver, l.ticks);
        const uint64_t after = fingerprint(w);
        CHECK(fingerprint(again) == after);  // the same save plays on the same way twice
        CHECK(after == l.playOn);

        // And the played-on world saves and loads back exactly.
        World back;
        CHECK(decodeSave(encodeSave(w), back, nullptr));
        CHECK(fingerprint(back) == after);

        // In playon.expect's format, so a deliberate rules change can be re-pinned from the log.
        std::printf("%s %s %" PRId64 " %016" PRIx64 " %016" PRIx64 "%s\n", l.fixture.c_str(), l.driver.c_str(), l.ticks,
                    load, after, load == l.load && after == l.playOn ? "" : "   <-- differs from playon.expect");
    }
    TEST_MAIN_END();
}
