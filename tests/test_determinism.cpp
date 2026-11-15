// Determinism: the same seed and inputs give the same world, and the
// fingerprint of a scripted walk is pinned so any change to the rules that
// alters the outcome shows up here (update the pin deliberately, in the same
// PR as the rule change, and say why).
#include <cinttypes>
#include <cmath>
#include <cstdio>

#include "check.h"
#include "sim/layout.h"
#include "sim/rules.h"
#include "strata/sim/noise.hpp"

using namespace kindling;

namespace {

// Walk in through the door, wander round the room looking about and sweeping, and stop.
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
        } else if (i > 4 * kTickRate) {
            // Sweep the floor a little ahead of the player as they wander.
            in.selectTool = static_cast<int32_t>(Tool::Broom);
            in.use = true;
            in.aim.kind = TargetKind::Floor;
            in.aim.point = {w.player.x + 0.5f * detSin(t), layout::kFloorY, w.player.z - 0.8f};
            in.aim.distance = 1.2f;
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

    // World generation draws grime from Strata's fBm (third_party/strata_noise). These
    // are the values Strata's own unit test pins (modules/sim/tests/test_sim.cpp at the
    // vendored commit), so the copy is checked against the original on every compiler.
    CHECK(strata::sim::latticeHash(0, 0, 0u) == 0x944fb554u);
    CHECK(strata::sim::latticeHash(-5, 7, 42u) == 0x5818a332u);
    {
        strata::sim::FbmParams p;
        p.seed = 1234u;
        p.octaves = 6;
        p.gain = 0.6f;
        const float xs[4] = {0.0f, 0.37f, -12.5f, 100.25f};
        const float noise[4] = {0.688756227f, 0.254520804f, -0.0458951704f, 0.0907022059f};
        const float frac[4] = {-0.169958144f, -0.153145447f, -0.296367735f, -0.0396123827f};
        for (int i = 0; i < 4; ++i) {
            CHECK(strata::sim::valueNoise(xs[i], xs[i] * 0.5f + 0.3f, 99u) == noise[i]);
            CHECK(strata::sim::fbm(xs[i], 1.75f - xs[i], p) == frac[i]);
        }
    }

    // The pinned fingerprint of the scripted walk (re-pinned when new games' grime
    // moved to fractal noise).
    const uint64_t pinned = 0x22152e88706a8cc3ULL;
    std::printf("scripted walk fingerprint: %016" PRIx64 "\n", fingerprint(a));
    CHECK(fingerprint(a) == pinned);

    TEST_MAIN_END();
}
