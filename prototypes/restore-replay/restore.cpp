// Kindling prototype: one cabin room restored step by step (Survey, Clear, Repair, Furnish,
// Kindle, Welcome), recorded as a usd-replay/1 replay with usd-recorder's C++ writer.
//
// Room coordinates are the Strata hearth scene's (x right, y up, z toward the camera); the
// replay is Z-up, so a room point (x, y, z) is written as (x, -z, y).
#include <usd_recorder/replay_writer.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using usd_recorder::Rgb8;

namespace {

struct Rng {  // PCG32
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
    uint32_t next() {
        uint64_t old = s;
        s = old * 6364136223846793005ULL + 1442695040888963407ULL;
        uint32_t x = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u), r = static_cast<uint32_t>(old >> 59u);
        return (x >> r) | (x << ((32 - r) & 31));
    }
    float uni(float a, float b) { return a + (b - a) * (next() >> 8) * (1.0f / 16777216.0f); }
};

struct Hasher {  // FNV-1a 64 over exact bit patterns
    uint64_t h = 1469598103934665603ULL;
    void bytes(const void* p, size_t n) {
        auto* b = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ULL; }
    }
    template <class T> void add(const T& v) { bytes(&v, sizeof v); }
};

// ---- floor grid: 32 x 28 cells of 0.25 m over x -4..4, z -3..4 (one row per floor plank)
constexpr int COLS = 32, ROWS = 28;
constexpr float CELL = 0.25f, X0 = -4.0f, Z0 = -3.0f;
enum Cell : int32_t { Dusty = 0, Clean = 1, Hole = 2, Snow = 3, NewPlank = 4 };

struct Item { int64_t id; float x, y, z, w; Rgb8 c; bool alive; };

struct Room {
    std::vector<int32_t> cell = std::vector<int32_t>(COLS * ROWS, Dusty);
    std::vector<Item> debris, shingles, boards, furniture, logs, sticks;
    float px = 2.5f, pz = 3.7f;           // the homesteader
    float sx = 2.5f, sz = 4.6f; bool settlerIn = false;
    int32_t step = 1;                      // 1 Survey .. 6 Welcome
    int32_t rug = 0, curtains = 0, quilt = 0, sampler = 0, mantel = 0, basket = 0, chairFixed = 0, tableUp = 0;
    float fire = 0.0f;                     // 0 cold .. 1 roaring
    int64_t fireTicks = 0;
};

int col(float x) { return std::max(0, std::min(COLS - 1, static_cast<int>(std::floor((x - X0) / CELL)))); }
int row(float z) { return std::max(0, std::min(ROWS - 1, static_cast<int>(std::floor((z - Z0) / CELL)))); }
float cx(int i) { return X0 + (i + 0.5f) * CELL; }
float cz(int j) { return Z0 + (j + 0.5f) * CELL; }
bool inHearthstone(int i, int j) { return cx(i) > -2.55f && cx(i) < 0.95f && cz(j) < -1.72f; }
bool inSnow(float x, float z) { float dx = (x - 0.1f) / 0.85f, dz = (z + 0.9f) / 0.65f; return dx * dx + dz * dz < 1.0f; }

void setupRoom(Room& r, Rng& rng) {
    // rotten planks (three rows, partly gone)
    struct Gap { float z, x0, x1; } gaps[] = {{-0.25f, -1.9f, -1.0f}, {0.75f, 0.3f, 1.2f}, {1.0f, 0.6f, 1.5f}};
    for (auto& g : gaps)
        for (int i = col(g.x0); i <= col(g.x1 - 0.01f); ++i) r.cell[row(g.z + 0.1f) * COLS + i] = Hole;
    for (int j = 0; j < ROWS; ++j)
        for (int i = 0; i < COLS; ++i)
            if (inSnow(cx(i), cz(j)) && r.cell[j * COLS + i] != Hole) r.cell[j * COLS + i] = Snow;
    int64_t id = 1;
    const Rgb8 leaf[] = {{122, 84, 44}, {109, 92, 58}, {86, 64, 42}};
    for (int k = 0; k < 70; ++k) {
        float x = rng.uni(-3.6f, 2.6f), z = rng.uni(-2.2f, 2.8f);
        r.debris.push_back({id++, x, 0.08f, z, rng.uni(0.06f, 0.11f), leaf[rng.next() % 3], true});
    }
    for (int k = 0; k < 9; ++k) {  // broken boards: three cubes in a line
        float x = rng.uni(-3.0f, 2.0f), z = rng.uni(-1.8f, 2.4f);
        for (int s = 0; s < 3; ++s) r.debris.push_back({id++, x + s * 0.2f, 0.1f, z, 0.16f, {98, 82, 66}, true});
    }
    for (int k = 0; k < 5; ++k) r.debris.push_back({id++, rng.uni(-0.6f, 0.9f), 0.09f, rng.uni(-1.6f, 0.6f), 0.18f, {74, 68, 62}, true});
    // the fallen rafter, as a line of cubes from the beam end to the floor
    for (int s = 0; s <= 8; ++s) {
        float t = s / 8.0f;
        r.debris.push_back({id++, 0.0f + (-1.9f - 0.0f) * t, 2.85f + (0.13f - 2.85f) * t, -0.2f + (0.5f + 0.2f) * t, 0.24f, {88, 70, 54}, true});
    }
    // window boards: four rows of cubes across the opening
    const float by[] = {1.14f, 1.44f, 1.75f, 2.03f};
    for (int b = 0; b < 4; ++b)
        for (int s = 0; s < 7; ++s) r.boards.push_back({100 + b * 10 + s, -3.95f, by[b], -1.15f + s * 0.29f, 0.24f, {104, 90, 70}, true});
    // the broken chair and the knocked-over table start in the room, grey with dust
    r.furniture.push_back({200, 1.65f, 0.35f, -1.15f, 0.5f, {80, 74, 68}, true});
    r.furniture.push_back({201, -3.3f, 0.06f, 1.6f, 0.6f, {80, 74, 68}, true});
}

// ---- jobs: walk to a spot, work there for some ticks, then change the room
struct Job { float x, z; int work; std::function<void(Room&)> done; int32_t step; bool sweep = false; };

std::vector<Job> plan(Room& r) {
    std::vector<Job> jobs;
    auto add = [&](float x, float z, int work, int32_t step, std::function<void(Room&)> f = nullptr) { jobs.push_back({x, z, work, std::move(f), step, false}); };
    // 1 Survey: walk the room
    for (auto [x, z] : std::vector<std::pair<float, float>>{{2.0f, 1.0f}, {-3.0f, 1.5f}, {-3.4f, -0.3f}, {-0.8f, -1.4f}, {1.6f, -1.8f}, {0.1f, -0.4f}}) add(x, z, 6, 1);
    // 2 Clear: the rafter, debris (nearest first, in loads of eight), the snow, then sweep
    add(-1.0f, 0.1f, 30, 2, [](Room& rm) { for (auto& d : rm.debris) if (d.w > 0.2f) d.alive = false; });
    {
        std::vector<size_t> order;
        for (size_t k = 0; k < r.debris.size(); ++k) if (r.debris[k].w <= 0.2f) order.push_back(k);
        float x = -1.0f, z = 0.1f;
        int carried = 0;
        while (!order.empty()) {
            size_t best = 0; float bd = 1e9f;
            for (size_t k = 0; k < order.size(); ++k) {
                float dx = r.debris[order[k]].x - x, dz = r.debris[order[k]].z - z, d = dx * dx + dz * dz;
                if (d < bd) { bd = d; best = k; }
            }
            size_t idx = order[best];
            order.erase(order.begin() + static_cast<long>(best));
            x = r.debris[idx].x; z = r.debris[idx].z;
            add(x, z, 1, 2, [idx](Room& rm) { rm.debris[idx].alive = false; });
            if (++carried % 12 == 0) { add(2.5f, 3.8f, 2, 2); x = 2.5f; z = 3.8f; }
        }
        add(2.5f, 3.8f, 3, 2);
    }
    for (int k = 0; k < 10; ++k) {  // shovel the drift out, armful by armful
        float a = k * 0.63f;
        add(0.1f + 0.55f * std::cos(a), -0.9f + 0.4f * std::sin(a), 3, 2, [k](Room& rm) {
            for (int j = 0; j < ROWS; ++j)
                for (int i = 0; i < COLS; ++i)
                    if (rm.cell[j * COLS + i] == Snow && ((i * 7 + j * 3) % 10) <= k) rm.cell[j * COLS + i] = Dusty;
        });
        add(2.5f, 3.8f, 1, 2);
    }
    for (int j = 1; j < ROWS; j += 3) {  // sweep, row by row
        bool lr = (j / 3) % 2 == 0;
        add(lr ? -3.7f : 3.7f, cz(j), 0, 2); jobs.back().sweep = true;
        add(lr ? 3.7f : -3.7f, cz(j), 0, 2); jobs.back().sweep = true;
    }
    // 3 Repair: roof, floor, window, chair, table
    for (int s = 0; s < 12; ++s) {
        add(0.1f, -0.9f, 7, 3, [s](Room& rm) {
            rm.shingles.push_back({300 + s, -0.5f + (s % 4) * 0.42f, 3.0f, -1.4f + (s / 4) * 0.42f, 0.42f, {92, 58, 42}, true});
        });
    }
    for (int j = 0; j < ROWS; ++j)
        for (int i = 0; i < COLS; ++i)
            if (r.cell[j * COLS + i] == Hole) add(cx(i), cz(j) + 0.35f, 4, 3, [i, j](Room& rm) { rm.cell[j * COLS + i] = NewPlank; });
    for (int b = 3; b >= 0; --b)
        add(-3.5f, -0.27f, 9, 3, [b](Room& rm) { for (auto& bd : rm.boards) if (bd.id / 10 == 10 + b) bd.alive = false; });
    add(1.65f, -0.6f, 30, 3, [](Room& rm) { rm.chairFixed = 1; });
    add(-3.0f, 1.0f, 12, 3, [](Room& rm) { rm.tableUp = 1; });
    // 4 Furnish
    add(-0.55f, -0.0f, 20, 4, [](Room& rm) { rm.rug = 1; });
    add(1.65f, -0.6f, 10, 4, [](Room& rm) { rm.quilt = 1; });
    add(-0.8f, -1.6f, 12, 4, [](Room& rm) { rm.sampler = 1; });
    add(-0.2f, -1.7f, 12, 4, [](Room& rm) { rm.mantel = 1; });
    add(-3.4f, -0.27f, 16, 4, [](Room& rm) { rm.curtains = 1; });
    for (int trip = 0; trip < 7; ++trip) {
        add(2.5f, 3.8f, 3, 4);
        add(-3.1f, -1.9f, 4, 4, [trip](Room& rm) {
            for (int k = 0; k < 2; ++k) {
                int n = trip * 2 + k, rowi = n < 5 ? 0 : n < 9 ? 1 : n < 12 ? 2 : 3, inRow = n - (rowi == 0 ? 0 : rowi == 1 ? 5 : rowi == 2 ? 9 : 12);
                rm.logs.push_back({400 + n, -3.45f + inRow * 0.17f + rowi * 0.085f, 0.08f + rowi * 0.15f, -2.4f, 0.16f, {150, 98, 58}, true});
            }
        });
    }
    add(0.95f, -1.5f, 8, 4, [](Room& rm) { rm.basket = 1; });
    // 5 Kindle: lay the kindling, strike, nurse it
    add(-0.8f, -1.55f, 6, 5, [](Room& rm) {
        for (int k = 0; k < 7; ++k) rm.sticks.push_back({500 + k, -1.1f + k * 0.1f, 0.12f, -2.58f + (k % 2) * 0.06f, 0.07f, {176, 135, 87}, true});
    });
    add(-0.8f, -1.55f, 4, 5, [](Room& rm) { rm.fire = 0.08f; });
    add(-0.8f, -1.55f, 40, 5);
    // 6 Welcome: step aside for the settler
    add(-2.8f, 0.6f, 4, 6, [](Room& rm) { rm.settlerIn = true; });
    return jobs;
}

void toReplay(float x, float y, float z, std::vector<float>& out) { out.push_back(x); out.push_back(-z); out.push_back(y); }

Rgb8 mix(Rgb8 a, Rgb8 b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    auto m = [t](uint8_t p, uint8_t q) { return static_cast<uint8_t>(std::lround(p + (q - p) * t)); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

std::vector<Rgb8> floorColors(const Room& r) {
    std::vector<Rgb8> c(COLS * ROWS);
    for (int j = 0; j < ROWS; ++j) {
        const uint8_t v = static_cast<uint8_t>((j * 37) % 17);
        for (int i = 0; i < COLS; ++i) {
            Rgb8 k;
            switch (r.cell[j * COLS + i]) {
                case Dusty: k = inHearthstone(i, j) ? Rgb8{104, 102, 98} : Rgb8{static_cast<uint8_t>(88 + v / 2), 82, 76}; break;
                case Clean: k = inHearthstone(i, j) ? Rgb8{142, 134, 122} : Rgb8{static_cast<uint8_t>(112 + v), static_cast<uint8_t>(72 + v / 2), 42}; break;
                case Hole: k = {24, 20, 18}; break;
                case Snow: k = {206, 214, 230}; break;
                default: k = {168, 118, 72}; break;  // a new plank, still pale
            }
            if (r.rug) {
                float dx = (cx(i) + 0.55f) / 1.4f, dz = (cz(j) + 0.55f) / 0.9f, d = std::sqrt(dx * dx + dz * dz);
                if (d < 1.0f) k = d < 0.22f ? Rgb8{176, 132, 74} : d < 0.45f ? Rgb8{70, 86, 122} : d < 0.62f ? Rgb8{120, 34, 26} : d < 0.82f ? Rgb8{176, 132, 74} : Rgb8{134, 40, 30};
            }
            if (r.fire > 0) {
                float dx = cx(i) + 0.8f, dz = cz(j) + 2.5f, d = std::sqrt(dx * dx + dz * dz);
                k = mix(k, {255, 150, 60}, r.fire * 0.55f * std::exp(-d / 1.6f));
            }
            c[(ROWS - 1 - j) * COLS + i] = k;  // replay Y = -z: field row 0 is the room's front
        }
    }
    return c;
}

uint64_t stateHash(const Room& r) {
    Hasher h;
    h.bytes(r.cell.data(), r.cell.size() * sizeof(int32_t));
    for (auto* v : {&r.debris, &r.shingles, &r.boards, &r.furniture, &r.logs, &r.sticks})
        for (auto& it : *v) { h.add(it.id); h.add(it.x); h.add(it.y); h.add(it.z); h.add(it.alive); }
    h.add(r.px); h.add(r.pz); h.add(r.sx); h.add(r.sz); h.add(r.settlerIn); h.add(r.step);
    for (int32_t f : {r.rug, r.curtains, r.quilt, r.sampler, r.mantel, r.basket, r.chairFixed, r.tableUp}) h.add(f);
    h.add(r.fire); h.add(r.fireTicks);
    return h.h;
}

}  // namespace

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "kindling-restore.usda";
    const uint64_t seed = 1880;
    Rng rng(seed);
    Room room;
    setupRoom(room, rng);
    std::vector<Job> jobs = plan(room);

    usd_recorder::ReplayMeta meta;
    meta.sim = "kindling-restore";
    meta.sim_version = "0.1.0-prototype";
    meta.seed = std::to_string(seed);
    meta.state_hash_algorithm = "kindling Room fingerprint (FNV-1a 64)";
    meta.tick_rate = 1.0 / 60.0;  // one tick is one minute of work
    meta.playback_rate = 90;
    meta.title = "Kindling: restoring the hearth room";
    meta.description = "One cabin room taken from ruin to home: survey, clear, repair, furnish, kindle, welcome.";
    usd_recorder::ReplayWriter w(meta);

    w.entity_set("homesteader", "You", Rgb8{240, 160, 70}, 0.45f, "sphere");
    w.entity_set("settler", "Ada Lark", Rgb8{170, 110, 190}, 0.45f, "sphere");
    w.entity_set("debris", "Debris", std::nullopt, std::nullopt, "cube");
    w.entity_set("boards", "Window boards", Rgb8{104, 90, 70}, 0.24f, "cube");
    w.entity_set("furniture", "Furniture", std::nullopt, std::nullopt, "cube");
    w.entity_set("firewood", "Firewood", Rgb8{150, 98, 58}, 0.16f, "cube");
    w.entity_set("fire", "Fire", std::nullopt, std::nullopt, "sphere");
    w.field("floor", "Floor", COLS, ROWS, X0, -(Z0 + ROWS * CELL), CELL, 0.0);

    // Static: cut-away log walls (three courses), the chimney breast and the window frame.
    {
        std::vector<std::array<float, 3>> pts;
        std::vector<Rgb8> cols;
        std::vector<float> widths;
        auto put = [&](float x, float y, float z, float wd, Rgb8 c) { pts.push_back({x, -z, y}); widths.push_back(wd); cols.push_back(c); };
        for (int course = 0; course < 3; ++course) {
            float y = 0.16f + course * 0.31f;
            Rgb8 c = course % 2 ? Rgb8{92, 58, 34} : Rgb8{104, 66, 38};
            for (float x = -4.1f; x <= 4.15f; x += 0.31f) { put(x, y, -3.12f, 0.31f, c); if (course == 0) put(x, y, 4.3f, 0.31f, c); }
            for (float z = -2.8f; z <= 4.2f; z += 0.31f) { put(-4.12f, y, z, 0.31f, c); put(4.12f, y, z, 0.31f, c); }
        }
        for (float y = 0.15f; y < 3.0f; y += 0.3f)
            for (float x = -2.15f; x <= 0.6f; x += 0.3f) {
                bool opening = x > -1.5f && x < -0.1f && y < 1.05f;
                if (!opening) put(x, y, -2.66f, 0.3f, (static_cast<int>(x * 10 + y * 7) % 2) ? Rgb8{120, 116, 108} : Rgb8{138, 132, 122});
            }
        for (float x = -2.5f; x <= 0.9f; x += 0.2f) put(x, 1.3f, -2.35f, 0.2f, {58, 32, 18});  // mantel
        put(-0.8f, 0.03f, -2.6f, 0.3f, {40, 32, 28});
        w.scene_points("structure", "Walls and hearth", pts, widths, cols, "cube");
    }

    size_t job = 0;
    int worked = 0;
    const float speed = 0.22f;
    int64_t endTick = -1;
    for (int64_t tick = 0;; ++tick) {
        // advance one minute
        if (job < jobs.size()) {
            Job& j = jobs[job];
            room.step = j.step;
            float dx = j.x - room.px, dz = j.z - room.pz, d = std::sqrt(dx * dx + dz * dz);
            if (d > speed) { room.px += dx / d * speed; room.pz += dz / d * speed; }
            else {
                room.px = j.x; room.pz = j.z;
                if (worked++ >= j.work) { if (j.done) j.done(room); ++job; worked = 0; }
            }
            if (j.sweep)  // the broom: dust under and around you comes clean
                for (int jj = row(room.pz - 0.5f); jj <= row(room.pz + 0.5f); ++jj)
                    for (int ii = col(room.px - 0.5f); ii <= col(room.px + 0.5f); ++ii)
                        if (room.cell[jj * COLS + ii] == Dusty && std::hypot(cx(ii) - room.px, cz(jj) - room.pz) < 0.55f)
                            room.cell[jj * COLS + ii] = Clean;
        } else if (endTick < 0) endTick = tick + 160;
        if (room.fire > 0) { room.fire = std::min(1.0f, room.fire + 0.012f); ++room.fireTicks; }
        if (room.settlerIn) {
            float tx = 1.65f, tz = -1.0f, dx = tx - room.sx, dz = tz - room.sz, d = std::sqrt(dx * dx + dz * dz);
            if (d > 0.18f) { room.sx += dx / d * 0.18f; room.sz += dz / d * 0.18f; } else { room.sx = tx; room.sz = tz; }
        }
        if (room.chairFixed && room.furniture.size() == 2) {
            room.furniture[0] = {200, 1.65f, 0.48f, -1.15f, 0.55f, {120, 72, 40}, true};
        }
        if (room.tableUp && room.furniture[1].y < 0.5f) room.furniture[1] = {201, -3.5f, 0.6f, 1.45f, 0.62f, {120, 70, 38}, true};

        if (tick % 4 != 0 && !(endTick >= 0 && tick >= endTick)) continue;

        // ---- sample
        char hash[17];
        std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(stateHash(room)));
        w.begin_sample(tick, hash);
        std::vector<int64_t> ids; std::vector<float> xyz; std::vector<Rgb8> cs; std::vector<float> ws;
        auto clear = [&] { ids.clear(); xyz.clear(); cs.clear(); ws.clear(); };
        auto put = [&](const Item& it) { if (!it.alive) return; ids.push_back(it.id); toReplay(it.x, it.y, it.z, xyz); cs.push_back(it.c); ws.push_back(it.w); };

        clear(); ids.push_back(1); toReplay(room.px, 0.45f, room.pz, xyz);
        w.entities("homesteader", ids.data(), xyz.data(), 1);
        clear(); if (room.settlerIn) { ids.push_back(1); toReplay(room.sx, 0.45f, room.sz, xyz); }
        w.entities("settler", ids.data(), xyz.data(), ids.size());
        clear(); for (auto& it : room.debris) put(it);
        w.entities("debris", ids.data(), xyz.data(), ids.size(), cs.data(), ws.data());
        clear(); for (auto& it : room.boards) put(it);
        w.entities("boards", ids.data(), xyz.data(), ids.size());
        clear();
        for (auto& it : room.furniture) put(it);
        if (room.chairFixed) put({202, 1.65f, 0.95f, -0.9f, 0.45f, {120, 72, 40}, true});
        if (room.quilt) { put({203, 1.5f, 0.9f, -0.86f, 0.18f, {150, 40, 34}, true}); put({204, 1.8f, 0.9f, -0.86f, 0.18f, {70, 96, 140}, true}); put({205, 1.65f, 0.72f, -0.86f, 0.18f, {200, 170, 110}, true}); }
        if (room.tableUp) put({206, -3.5f, 0.98f, 1.75f, 0.3f, {70, 96, 140}, true});
        if (room.sampler) put({207, -0.8f, 2.1f, -2.45f, 0.5f, {200, 186, 150}, true});
        if (room.mantel) { put({208, -2.2f, 1.47f, -2.3f, 0.16f, {170, 150, 100}, true}); put({209, -0.05f, 1.52f, -2.35f, 0.26f, {90, 52, 28}, true}); put({210, 0.55f, 1.47f, -2.3f, 0.08f, {240, 232, 210}, true}); }
        if (room.curtains) { put({211, -3.9f, 1.7f, -1.25f, 0.4f, {120, 30, 26}, true}); put({212, -3.9f, 1.7f, 0.68f, 0.4f, {120, 30, 26}, true}); }
        if (room.basket) put({213, 0.95f, 0.22f, -1.95f, 0.4f, {150, 104, 52}, true});
        for (auto& it : room.sticks) put(it);
        w.entities("furniture", ids.data(), xyz.data(), ids.size(), cs.data(), ws.data());
        clear(); for (auto& it : room.logs) put(it);
        w.entities("firewood", ids.data(), xyz.data(), ids.size());
        clear();
        if (room.fire > 0) {  // flames: count and height grow with the fire; they flicker every sample
            int n = 2 + static_cast<int>(room.fire * 9);
            for (int k = 0; k < n; ++k) {
                float h = rng.uni(0.15f, 0.25f + 0.45f * room.fire);
                ids.push_back(600 + k);
                toReplay(-0.8f + rng.uni(-0.42f, 0.42f) * (0.5f + 0.5f * room.fire), h, -2.6f + rng.uni(-0.12f, 0.12f), xyz);
                cs.push_back(mix({255, 110, 30}, {255, 225, 120}, (h - 0.15f) < 0.2f ? 0.8f : 0.2f));
                ws.push_back(rng.uni(0.12f, 0.2f + 0.16f * room.fire));
            }
        }
        w.entities("fire", ids.data(), xyz.data(), ids.size(), cs.data(), ws.data());
        auto f = floorColors(room);
        w.field_colors("floor", f.data());

        int debris = 0, holes = 0, dusty = 0;
        for (auto& d : room.debris) debris += d.alive;
        for (int32_t c : room.cell) { holes += c == Hole; dusty += c == Dusty || c == Snow; }
        int furnished = room.rug + room.quilt + room.sampler + room.mantel + room.curtains + room.basket + room.chairFixed + room.tableUp + static_cast<int>(room.logs.size() / 2);
        w.stat("step", room.step);
        w.stat("debris_left", debris);
        w.stat("floor_holes", holes);
        w.stat("dusty_cells", dusty);
        w.stat("roof_patched_pct", room.shingles.size() * 100.0 / 12.0);
        w.stat("furnished", furnished);
        w.stat("warmth", std::round(room.fire * 100.0));
        w.end_sample();
        if (endTick >= 0 && tick >= endTick) break;
    }
    w.write(out);
    std::printf("wrote %s: %zu samples\n", out, w.samples());
    return 0;
}
