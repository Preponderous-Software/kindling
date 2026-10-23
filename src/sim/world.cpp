#include "sim/world.h"

#include <algorithm>
#include <cstring>

#include "sim/layout.h"
#include "sim/progress.h"
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
    w.items = newItems();
    w.history.push_back(snapshot(w));
    return w;
}

double gameMinutes(const World& w) {
    return kStartMinute + static_cast<double>(w.tick) / kTickRate * kGameMinutesPerSecond;
}

double timeOfDay(const World& w) {
    const double m = gameMinutes(w);
    return (m - 1440.0 * static_cast<double>(static_cast<int64_t>(m / 1440.0))) / 1440.0;
}

int32_t dayNumber(const World& w) { return static_cast<int32_t>(gameMinutes(w) / 1440.0); }

bool frosted(const World& w) { return gameMinutes(w) >= 20.0 * 60.0; }
bool dusk(const World& w) {
    const double t = timeOfDay(w);
    return t >= 16.5 / 24.0 || t < 7.0 / 24.0;
}

const char* woodName(Wood k) {
    switch (k) {
        case Wood::Round: return "a log round";
        case Wood::Split: return "a split log";
        case Wood::Kindling: return "kindling";
    }
    return "";
}

const char* toolName(Tool t) {
    switch (t) {
        case Tool::None: return "Bare hands";
        case Tool::Broom: return "Broom";
        case Tool::ScrubBrush: return "Scrub brush";
        case Tool::Shovel: return "Shovel";
        case Tool::Saw: return "Saw";
        case Tool::PryBar: return "Pry bar";
        case Tool::Hammer: return "Hammer";
        case Tool::Axe: return "Axe";
        case Tool::Matches: return "Matches";
        case Tool::CurryComb: return "Curry comb";
        case Tool::Count: break;
    }
    return "";
}

Keyframe snapshot(const World& w) {
    Keyframe k;
    k.tick = w.tick;
    k.step = static_cast<int32_t>(progress(w).step);
    k.lantern = w.lantern;
    k.playerX = w.player.x;
    k.playerZ = w.player.z;
    k.playerYaw = w.player.yaw;
    for (int s = 0; s < kSurfaceCount; ++s) {
        const SurfaceDef& d = surfaceDef(static_cast<Surface>(s));
        const int hw = (d.width + 1) / 2, hh = (d.height + 1) / 2;
        std::vector<uint8_t>& out = k.grime[static_cast<size_t>(s)];
        out.resize(static_cast<size_t>(hw * hh));
        for (int j = 0; j < hh; ++j)
            for (int i = 0; i < hw; ++i)
                out[static_cast<size_t>(j * hw + i)] = w.grime.at(static_cast<Surface>(s), std::min(2 * i, d.width - 1),
                                                                  std::min(2 * j, d.height - 1));
    }
    k.debris = w.debris;
    k.planks = w.planks;
    k.windowBoards = w.windowBoards;
    k.roofPatched = w.roofPatched;
    k.chair = w.chair;
    k.table = w.table;
    k.snow = w.snow;
    k.items = w.items;
    k.hearth = w.hearth;
    k.woodpile = w.wood.woodpile;
    k.settler = w.settler;
    return k;
}

World fromKeyframe(const World& base, const Keyframe& k) {
    World w = base;
    w.tick = k.tick;
    w.player.x = k.playerX;
    w.player.z = k.playerZ;
    w.player.yaw = k.playerYaw;
    w.carry = Carry::Nothing;
    w.carryIndex = -1;
    for (int s = 0; s < kSurfaceCount; ++s) {
        const SurfaceDef& d = surfaceDef(static_cast<Surface>(s));
        const int hw = (d.width + 1) / 2;
        const std::vector<uint8_t>& in = k.grime[static_cast<size_t>(s)];
        std::vector<uint8_t>& out = w.grime.texels[static_cast<size_t>(s)];
        out.resize(static_cast<size_t>(d.width * d.height));
        for (int j = 0; j < d.height; ++j)
            for (int i = 0; i < d.width; ++i) out[static_cast<size_t>(j * d.width + i)] = in[static_cast<size_t>((j / 2) * hw + i / 2)];
    }
    w.debris = k.debris;
    w.planks = k.planks;
    w.windowBoards = k.windowBoards;
    w.roofPatched = k.roofPatched;
    w.chair = k.chair;
    w.table = k.table;
    w.snow = k.snow;
    w.items = k.items;
    for (Item& it : w.items)
        if (it.place == ItemPlace::Carried) it.place = ItemPlace::Wagon;  // the reveal shows nothing in mid-air
    w.hearth = k.hearth;
    w.wood.woodpile = k.woodpile;
    w.settler = k.settler;
    w.lantern = k.lantern;
    // Steps as they were then (the latches feed progress()).
    w.cleared = k.step > static_cast<int32_t>(Step::Clear);
    w.repaired = k.step > static_cast<int32_t>(Step::Repair);
    w.kindled = k.step > static_cast<int32_t>(Step::Kindle);
    w.history.clear();
    return w;
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
    f.add(w.roofPatched);
    f.add(w.chair);
    f.add(w.table);
    f.add(w.snow);
    f.add(w.debrisHauled);
    f.add(static_cast<uint64_t>(w.items.size()));
    for (const Item& it : w.items) {
        f.add(it.id);
        f.add(it.kind);
        f.add(it.place);
        f.add(it.pos.x);
        f.add(it.pos.y);
        f.add(it.pos.z);
        f.add(it.yaw);
    }
    const Hearth& h = w.hearth;
    f.add(h.ash);
    f.add(h.kindlingLaid);
    f.add(h.logsLaid);
    f.add(h.lit);
    f.add(h.fire);
    f.add(h.fuelMinutes);
    f.add(h.candleLit);
    f.add(h.matchesStruck);
    f.add(w.wood.rounds);
    f.add(w.wood.roundOnBlock);
    f.add(w.wood.splitByBlock);
    f.add(w.wood.kindlingByBlock);
    f.add(w.wood.woodpile);
    f.add(w.lantern);
    f.add(w.horse.dirt);
    f.add(w.horse.hunger);
    f.add(w.horse.eatingSeconds);
    f.add(w.horse.brushedSeconds);
    f.add(w.horse.greeted);
    const Settler& st = w.settler;
    f.add(st.state);
    f.add(st.x);
    f.add(st.z);
    f.add(st.yaw);
    f.add(st.waypoint);
    f.add(st.line);
    f.add(st.lineSeconds);
    f.add(st.briefMet);
    f.add(st.done);
    f.add(static_cast<uint64_t>(w.history.size()));
    for (const Keyframe& k : w.history) {
        f.add(k.tick);
        f.add(k.step);
        f.add(k.lantern);
        f.add(k.playerX);
        f.add(k.playerZ);
        for (const std::vector<uint8_t>& g : k.grime) f.bytes(g.data(), g.size());
        f.add(static_cast<uint64_t>(k.debris.size()));
        f.add(k.roofPatched);
        f.add(k.hearth.fire);
    }
    f.add(w.cleared);
    f.add(w.repaired);
    f.add(w.kindled);
    for (const std::vector<uint8_t>& t : w.grime.texels) {
        f.add(static_cast<uint64_t>(t.size()));
        f.bytes(t.data(), t.size());
    }
    f.add(w.surveyed);
    return f.h;
}

}  // namespace kindling
