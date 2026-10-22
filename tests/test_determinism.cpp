// Determinism: the same seed and inputs give the same world, and the
// fingerprint of a scripted walk is pinned so any change to the rules that
// alters the outcome shows up here (update the pin deliberately, in the same
// PR as the rule change, and say why).
#include <cinttypes>
#include <cstdio>

#include "check.h"
#include "sim/rules.h"

using namespace kindling;

namespace {

// Walk in through the door, wander round the room looking about, and stop.
World scriptedWalk(uint64_t seed) {
    World w = newWorld(seed);
    for (int i = 0; i < 20 * kTickRate; ++i) {
        Input in;
        const float t = static_cast<float>(i) / kTickRate;
        in.forward = i < 3 * kTickRate ? 1.0f : 0.6f;
        in.strafe = i % 90 < 45 ? 0.3f : -0.3f;
        in.sprint = i % 150 < 20;
        in.yaw = i < 3 * kTickRate ? 0.0f : 0.4f * t;
        in.pitch = 0.2f * static_cast<float>((i / 30) % 5) - 0.4f;
        if (i % 60 == 0) {
            in.aim.kind = TargetKind::Debris;
            in.aim.index = (i / 60) % 15;
            in.aim.distance = 1.0f;
        }
        tick(w, in);
    }
    return w;
}

}  // namespace

int main() {
    const World a = scriptedWalk(1881), b = scriptedWalk(1881);
    CHECK(fingerprint(a) == fingerprint(b));
    CHECK(fingerprint(newWorld(1)) != fingerprint(newWorld(2)));

    // The pinned fingerprint of the scripted walk.
    const uint64_t pinned = 0x1c0d2153e30ca0f5ULL;
    std::printf("scripted walk fingerprint: %016" PRIx64 "\n", fingerprint(a));
    CHECK(fingerprint(a) == pinned);

    TEST_MAIN_END();
}
