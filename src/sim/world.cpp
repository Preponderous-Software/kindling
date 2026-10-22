#include "sim/world.h"

#include <cstring>

#include "sim/layout.h"
#include "sim/rng.h"

namespace kindling {

World newWorld(uint64_t seed) {
    World w;
    w.seed = seed;
    w.player.x = layout::kSpawn.x;
    w.player.z = layout::kSpawn.z;
    w.player.yaw = layout::kSpawnYaw;

    // Broken boards and fallen shingles on the floor, and the rafter that came
    // down with the roof.
    Rng rng(seed);
    int32_t id = 1;
    for (int k = 0; k < 9; ++k) {
        Debris d;
        d.id = id++;
        d.kind = DebrisKind::Board;
        d.pos = {rng.uniform(-3.0f, 2.2f), layout::kFloorY + 0.03f, rng.uniform(-1.6f, 3.4f)};
        d.yaw = rng.uniform(0.0f, 3.14159265f);
        w.debris.push_back(d);
    }
    for (int k = 0; k < 5; ++k) {
        Debris d;
        d.id = id++;
        d.kind = DebrisKind::Shingle;
        d.pos = {rng.uniform(-0.6f, 0.9f), layout::kFloorY + 0.015f, rng.uniform(-1.6f, 0.6f)};
        d.yaw = rng.uniform(0.0f, 3.14159265f);
        w.debris.push_back(d);
    }
    Debris rafter;
    rafter.id = id++;
    rafter.kind = DebrisKind::Rafter;
    // Its foot rests on the floor; it leans up to the edge of the roof hole.
    rafter.pos = {-1.6f, layout::kFloorY + 0.11f, 0.4f};
    rafter.yaw = 0.532f;
    w.debris.push_back(rafter);

    w.planks.fill(PlankState::Rotten);
    w.windowBoards.fill(1);
    w.grime = newGrime(seed);
    return w;
}

const char* toolName(Tool t) {
    switch (t) {
        case Tool::None: return "Bare hands";
        case Tool::Broom: return "Broom";
        case Tool::ScrubBrush: return "Scrub brush";
        case Tool::Shovel: return "Shovel";
        case Tool::Saw: return "Saw";
        case Tool::Count: break;
    }
    return "";
}

namespace {

struct Fnv {
    uint64_t h = 1469598103934665603ULL;
    void bytes(const void* p, size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) {
            h ^= b[i];
            h *= 1099511628211ULL;
        }
    }
    template <class T>
    void add(const T& v) {
        unsigned char raw[sizeof(T)];
        std::memcpy(raw, &v, sizeof(T));
        bytes(raw, sizeof(T));
    }
};

}  // namespace

uint64_t fingerprint(const World& w) {
    Fnv f;
    f.add(w.tick);
    f.add(w.seed);
    f.add(w.player.x);
    f.add(w.player.z);
    f.add(w.player.yaw);
    f.add(w.player.pitch);
    f.add(w.tool);
    f.add(w.carry);
    f.add(w.carryIndex);
    f.add(w.workKind);
    f.add(w.workIndex);
    f.add(w.workSeconds);
    f.add(static_cast<uint64_t>(w.debris.size()));
    for (const Debris& d : w.debris) {
        f.add(d.id);
        f.add(d.kind);
        f.add(d.pos.x);
        f.add(d.pos.y);
        f.add(d.pos.z);
        f.add(d.yaw);
        f.add(d.hauled);
    }
    for (PlankState p : w.planks) f.add(p);
    for (int32_t b : w.windowBoards) f.add(b);
    f.add(w.roofPatches);
    f.add(w.chair);
    f.add(w.table);
    f.add(w.snow);
    f.add(w.debrisHauled);
    for (const std::vector<uint8_t>& t : w.grime.texels) {
        f.add(static_cast<uint64_t>(t.size()));
        f.bytes(t.data(), t.size());
    }
    f.add(w.surveyed);
    return f.h;
}

}  // namespace kindling
