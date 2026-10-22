// The whole MVP, start to finish: the autopilot takes the ruin through every
// step to Ada sitting by the fire, through the same Input a player gives. The
// fingerprint of the finished world is pinned, and the reveal's history covers
// the whole restoration.
#include <cinttypes>
#include <cstdio>

#include "check.h"
#include "sim/autopilot.h"
#include "sim/brief.h"
#include "sim/progress.h"
#include "sim/settler.h"

using namespace kindling;

int main() {
    World w = newWorld(1881);
    Autopilot pilot;
    Step last = Step::Survey;
    const int64_t limit = 60LL * 60 * kTickRate;  // an hour of play at most
    while (progress(w).step != Step::Done && w.tick < limit) {
        tick(w, pilot.next(w));
        const Step now = progress(w).step;
        if (now != last) {
            std::printf("%6.1f s  %-8s (%s)\n", static_cast<double>(w.tick) / kTickRate, stepName(now),
                        pilot.activity().c_str());
            last = now;
        }
    }
    const Progress p = progress(w);
    CHECK(p.step == Step::Done);
    CHECK(w.cleared && w.repaired && w.kindled);
    CHECK(w.settler.done && w.settler.state == SettlerState::Seated);
    CHECK(evaluateBrief(w).met());
    CHECK(w.settler.briefMet);
    CHECK(w.hearth.lit && w.hearth.candleLit);
    CHECK(w.history.size() > 10);
    CHECK(w.history.front().tick == 0 && w.history.back().tick == w.tick);

    std::printf("finished in %.1f minutes of play, %zu keyframes, %d matches\n",
                static_cast<double>(w.tick) / kTickRate / 60.0, w.history.size(), w.hearth.matchesStruck);
    const uint64_t pinned = 0xa5d50cfe6efe8202ULL;
    std::printf("playthrough fingerprint: %016" PRIx64 "\n", fingerprint(w));
    CHECK(fingerprint(w) == pinned);
    TEST_MAIN_END();
}
