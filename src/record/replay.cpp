#include "record/replay.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <vector>

#include "sim/layout.h"
#include "usd_recorder/replay_writer.hpp"

namespace kindling {

namespace L = kindling::layout;
using usd_recorder::Rgb8;

namespace {

// The floor as a coarse colour field: 32 x 28 cells of 25 cm over the room.
constexpr int kCols = 32, kRows = 28;
constexpr float kCell = 0.25f, kX0 = -4.0f, kZ0 = -3.0f;

// Room coordinates are y-up; the replay is Z-up: (x, y, z) -> (x, -z, y).
void put(std::vector<float>& out, float x, float y, float z) {
    out.push_back(x);
    out.push_back(-z);
    out.push_back(y);
}

Rgb8 mix(Rgb8 a, Rgb8 b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto m = [t](uint8_t p, uint8_t q) { return static_cast<uint8_t>(std::lround(p + (q - p) * t)); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

uint8_t keyGrime(const Keyframe& k, Surface s, float x, float z) {
    const SurfaceDef& d = surfaceDef(s);
    const int hw = (d.width + 1) / 2, hh = (d.height + 1) / 2;
    const float u = (x - d.origin.x) / d.uLength, v = (z - d.origin.z) / d.vLength;
    if (u < 0 || u >= 1 || v < 0 || v >= 1) return 0;
    const int i = std::min(hw - 1, static_cast<int>(u * hw)), j = std::min(hh - 1, static_cast<int>(v * hh));
    return k.grime[static_cast<size_t>(s)][static_cast<size_t>(j * hw + i)];
}

const Item* placed(const Keyframe& k, ItemKind kind) {
    for (const Item& it : k.items)
        if (it.kind == kind && it.place == ItemPlace::Placed) return &it;
    return nullptr;
}

std::vector<Rgb8> floorField(const Keyframe& k) {
    std::vector<Rgb8> c(kCols * kRows);
    const Item* rug = placed(k, ItemKind::Rug);
    for (int j = 0; j < kRows; ++j) {
        const float z = kZ0 + (static_cast<float>(j) + 0.5f) * kCell;
        for (int i = 0; i < kCols; ++i) {
            const float x = kX0 + (static_cast<float>(i) + 0.5f) * kCell;
            const uint8_t v = static_cast<uint8_t>((j * 37) % 17);
            Rgb8 col = mix({static_cast<uint8_t>(112 + v), static_cast<uint8_t>(72 + v / 2), 42}, {92, 86, 78},
                           static_cast<float>(keyGrime(k, Surface::Floor, x, z)) / 200.0f);
            if (x > -2.55f && x < 0.95f && z < L::kHearthstoneZ1) col = {140, 132, 120};  // the hearthstone
            for (size_t s = 0; s < L::kRottenSpans.size(); ++s) {
                const L::RottenSpan& r = L::kRottenSpans[s];
                if (x >= r.x0 && x <= r.x1 && z >= L::plankZ0(r.row) && z < L::plankZ0(r.row + 1)) {
                    if (k.planks[s] == PlankState::Rotten) col = {40, 30, 22};
                    else if (k.planks[s] == PlankState::Removed) col = {20, 16, 14};
                    else col = {168, 118, 72};
                }
            }
            const float sx = (x - 0.1f) / (0.45f + 0.35f * k.snow), sz = (z + 0.9f) / (0.35f + 0.3f * k.snow);
            if (k.snow > 0 && sx * sx + sz * sz < 1.0f) col = {206, 214, 230};
            if (rug) {
                const float rc = std::cos(rug->yaw), rs = std::sin(rug->yaw);
                const float lx = (x - rug->pos.x) * rc - (z - rug->pos.z) * rs;
                const float lz = (x - rug->pos.x) * rs + (z - rug->pos.z) * rc;
                const float d = std::sqrt((lx / 1.4f) * (lx / 1.4f) + (lz / 0.9f) * (lz / 0.9f));
                if (d < 1.0f)
                    col = d < 0.28f ? Rgb8{176, 132, 74} : d < 0.45f ? Rgb8{70, 86, 122} : d < 0.68f ? Rgb8{120, 34, 26}
                          : d < 0.82f ? Rgb8{176, 132, 74} : Rgb8{134, 40, 30};
            }
            if (k.hearth.lit) {
                const float dx = x + 0.8f, dz = z + 2.5f;
                col = mix(col, {255, 150, 60}, k.hearth.fire * 0.55f * std::exp(-std::sqrt(dx * dx + dz * dz) / 1.6f));
            }
            // Field row 0 is the minimum replay Y, which is the room's front (Y = -z).
            c[static_cast<size_t>((kRows - 1 - j) * kCols + i)] = col;
        }
    }
    return c;
}

uint64_t frameHash(const Keyframe& k) {
    uint64_t h = 1469598103934665603ULL;
    auto bytes = [&h](const void* p, size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (size_t i = 0; i < n; ++i) {
            h ^= b[i];
            h *= 1099511628211ULL;
        }
    };
    bytes(&k.tick, sizeof k.tick);
    bytes(&k.playerX, sizeof k.playerX);
    bytes(&k.playerZ, sizeof k.playerZ);
    for (const std::vector<uint8_t>& g : k.grime) bytes(g.data(), g.size());
    for (const Debris& d : k.debris) bytes(&d.hauled, sizeof d.hauled);
    bytes(k.planks.data(), sizeof k.planks);
    bytes(&k.roofPatched, sizeof k.roofPatched);
    for (const Item& it : k.items) bytes(&it.place, sizeof it.place);
    bytes(&k.hearth.fire, sizeof k.hearth.fire);
    bytes(&k.settler.x, sizeof k.settler.x);
    bytes(&k.settler.z, sizeof k.settler.z);
    return h;
}

struct FurnishingLook {
    float width;
    Rgb8 color;
    float lift;  // centre above its anchor point
};

FurnishingLook lookOf(ItemKind k) {
    switch (k) {
        case ItemKind::Quilt: return {0.45f, {150, 40, 34}, 0.0f};
        case ItemKind::Curtains: return {0.4f, {120, 30, 26}, -0.6f};
        case ItemKind::Sampler: return {0.5f, {200, 186, 150}, 0.0f};
        case ItemKind::Crock: return {0.16f, {170, 150, 100}, 0.12f};
        case ItemKind::Jar: return {0.12f, {80, 120, 150}, 0.1f};
        case ItemKind::Clock: return {0.26f, {90, 52, 28}, 0.16f};
        case ItemKind::Candle: return {0.08f, {240, 232, 210}, 0.12f};
        case ItemKind::Basket: return {0.4f, {150, 104, 52}, 0.2f};
        default: return {0, {}, 0};
    }
}

}  // namespace

bool writeReplay(const World& w, const std::string& path, std::string* error) {
    if (w.history.empty()) {
        if (error) *error = "no restoration history to write";
        return false;
    }
    try {
        usd_recorder::ReplayMeta meta;
        meta.sim = "kindling";
        meta.sim_version = "0.1.0";
        meta.seed = std::to_string(w.seed);
        meta.state_hash_algorithm = "export-frame FNV-1a 64 over the reveal keyframe";
        meta.tick_rate = kTickRate;
        const double span = static_cast<double>(w.history.back().tick);
        meta.playback_rate = std::max(30.0, span / 40.0);  // about forty seconds to play
        meta.title = "Kindling: a restoration";
        meta.description = "One cabin taken from ruin to home, recorded as it was played.";
        usd_recorder::ReplayWriter rw(meta);

        rw.entity_set("homesteader", "You", Rgb8{240, 160, 70}, 0.45f, "sphere");
        rw.entity_set("settler", "Ada Lark", Rgb8{170, 110, 190}, 0.45f, "sphere");
        rw.entity_set("debris", "Debris", Rgb8{104, 88, 70}, 0.18f, "cube");
        rw.entity_set("furniture", "Furnishings", std::nullopt, std::nullopt, "cube");
        rw.entity_set("fire", "Fire", Rgb8{255, 150, 50}, std::nullopt, "sphere");
        rw.field("floor", "Floor", kCols, kRows, kX0, -(kZ0 + kRows * kCell), kCell, 0.0);

        // Static: cut-away log walls, the chimney breast and the mantel.
        std::vector<std::array<float, 3>> pts;
        std::vector<float> widths;
        std::vector<Rgb8> cols;
        auto wall = [&](float x, float y, float z, float wd, Rgb8 c) {
            pts.push_back({x, -z, y});
            widths.push_back(wd);
            cols.push_back(c);
        };
        for (int course = 0; course < 3; ++course) {
            const float y = 0.16f + static_cast<float>(course) * 0.31f;
            const Rgb8 c = course % 2 ? Rgb8{92, 58, 34} : Rgb8{104, 66, 38};
            for (float x = -4.1f; x <= 4.15f; x += 0.31f) {
                wall(x, y, -3.12f, 0.31f, c);
                if (course == 0 && (x < L::kDoorX0 || x > L::kDoorX1)) wall(x, y, 4.3f, 0.31f, c);
            }
            for (float z = -2.8f; z <= 4.2f; z += 0.31f) {
                wall(-4.12f, y, z, 0.31f, c);
                wall(4.12f, y, z, 0.31f, c);
            }
        }
        for (float y = 0.15f; y < 3.0f; y += 0.3f)
            for (float x = -2.15f; x <= 0.6f; x += 0.3f)
                if (!(x > L::kFireboxX0 && x < L::kFireboxX1 && y < L::kFireboxTop)) wall(x, y, -2.66f, 0.3f, {130, 124, 114});
        rw.scene_points("structure", "Walls and hearth", pts, widths, cols, "cube");

        for (const Keyframe& k : w.history) {
            char hash[17];
            std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(frameHash(k)));
            rw.begin_sample(k.tick, hash);
            std::vector<int64_t> ids;
            std::vector<float> xyz, ws;
            std::vector<Rgb8> cs;
            auto clear = [&] {
                ids.clear();
                xyz.clear();
                ws.clear();
                cs.clear();
            };

            ids.push_back(1);
            put(xyz, k.playerX, 0.45f, k.playerZ);
            rw.entities("homesteader", ids.data(), xyz.data(), 1);

            clear();
            if (k.settler.state != SettlerState::Away) {
                ids.push_back(1);
                put(xyz, k.settler.x, 0.45f, k.settler.z);
            }
            rw.entities("settler", ids.data(), xyz.data(), ids.size());

            clear();
            for (const Debris& d : k.debris)
                if (!d.hauled && L::kInterior.contains(d.pos.x, d.pos.z)) {
                    ids.push_back(d.id);
                    put(xyz, d.pos.x, std::max(d.pos.y, 0.1f), d.pos.z);
                }
            rw.entities("debris", ids.data(), xyz.data(), ids.size());

            clear();
            for (const Item& it : k.items) {
                if (it.place != ItemPlace::Placed || it.kind == ItemKind::Rug) continue;  // the rug is in the floor field
                const FurnishingLook look = lookOf(it.kind);
                if (look.width <= 0) continue;
                ids.push_back(it.id);
                put(xyz, it.pos.x, it.pos.y + look.lift, it.pos.z);
                ws.push_back(look.width);
                cs.push_back(look.color);
            }
            if (k.chair == ChairState::Mended) {
                ids.push_back(1000);
                put(xyz, 1.65f, 0.6f, -1.15f);
                ws.push_back(0.55f);
                cs.push_back({120, 72, 40});
            }
            if (k.table == TableState::Upright) {
                ids.push_back(1001);
                put(xyz, -3.5f, 0.6f, 1.45f);
                ws.push_back(0.6f);
                cs.push_back({120, 70, 38});
            }
            rw.entities("furniture", ids.data(), xyz.data(), ids.size(), cs.data(), ws.data());

            clear();
            if (k.hearth.lit && k.hearth.fire > 0.01f) {
                const int n = 2 + static_cast<int>(k.hearth.fire * 6);
                for (int f = 0; f < n; ++f) {
                    const float t = static_cast<float>(k.tick) * 0.37f + static_cast<float>(f) * 1.7f;
                    ids.push_back(600 + f);
                    put(xyz, -0.8f + 0.35f * detSin(t), 0.2f + 0.3f * k.hearth.fire * (0.5f + 0.5f * detSin(t * 1.3f)),
                        -2.6f + 0.08f * detCos(t));
                    ws.push_back(0.12f + 0.12f * k.hearth.fire);
                }
            }
            rw.entities("fire", ids.data(), xyz.data(), ids.size(), nullptr, ws.data());

            const std::vector<Rgb8> field = floorField(k);
            rw.field_colors("floor", field.data());

            int debrisLeft = 0;
            for (const Debris& d : k.debris) debrisLeft += d.hauled ? 0 : 1;
            int furnished = 0;
            for (const Item& it : k.items) furnished += !isMaterial(it.kind) && it.place == ItemPlace::Placed ? 1 : 0;
            int patched = 0;
            for (int b = 0; b < 4; ++b) patched += (k.roofPatched >> b) & 1u ? 1 : 0;
            rw.stat("debris_left", debrisLeft);
            rw.stat("roof_patched", patched);
            rw.stat("furnished", furnished);
            rw.stat("fire", std::round(k.hearth.fire * 100.0f));
            rw.end_sample();
        }
        rw.write(path);
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
    return true;
}

}  // namespace kindling
