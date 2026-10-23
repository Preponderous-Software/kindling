#include "sim/autopilot.h"

#include <cmath>

#include "sim/layout.h"
#include "sim/progress.h"

namespace kindling {

namespace L = kindling::layout;

namespace {

constexpr float kArrive = 0.2f;        // close enough to a standing spot
constexpr int kTexelPatience = 3 * kTickRate;  // give up on a grime texel after this long at it
constexpr int kStuckTicks = 2 * kTickRate;     // walking without moving this long means stuck

// The door, from either side.
constexpr Vec2 kDoorInside{2.5f, 3.6f}, kDoorOutside{2.5f, 5.0f};

bool inside(float x, float z) { return L::kInterior.contains(x, z); }

Vec3 eyeOf(const World& w) { return {w.player.x, L::kEyeHeight, w.player.z}; }

Aim aimAt(const World& w, TargetKind kind, int32_t index, Vec3 point) {
    Aim a;
    a.kind = kind;
    a.index = index;
    a.point = point;
    a.distance = length(point - eyeOf(w));
    return a;
}

// Keeps a standing spot inside the room and out of the chimney.
Vec2 clampIndoors(float x, float z) {
    x = clampf(x, L::kInnerX0 + 0.4f, L::kInnerX1 - 0.4f);
    z = clampf(z, L::kInnerZ0 + 0.4f, L::kInnerZ1 - 0.4f);
    if (x > L::kBreastX0 - 0.35f && x < L::kBreastX1 + 0.35f && z < L::kBreastFront + 0.4f) z = L::kBreastFront + 0.4f;
    return {x, z};
}

Vec3 texelPoint(Surface s, int i, int j) {
    const SurfaceDef& d = surfaceDef(s);
    return d.origin + d.u * ((static_cast<float>(i) + 0.5f) / static_cast<float>(d.width) * d.uLength) +
           d.v * ((static_cast<float>(j) + 0.5f) / static_cast<float>(d.height) * d.vLength);
}

// Where to stand to work on a point of a surface.
Vec2 standFor(Surface s, Vec3 p) {
    switch (s) {
        case Surface::Floor: {
            // A metre back from it, toward the middle of the room.
            const float dx = 0.0f - p.x, dz = 0.6f - p.z, d = std::sqrt(dx * dx + dz * dz);
            return d < 1e-3f ? clampIndoors(p.x, p.z + 1.0f) : clampIndoors(p.x + dx / d, p.z + dz / d);
        }
        case Surface::BackWall: return clampIndoors(p.x, p.z + 0.9f);
        case Surface::LeftWall: return clampIndoors(p.x + 0.9f, p.z);
        case Surface::RightWall: return clampIndoors(p.x - 0.9f, p.z);
        case Surface::FrontWall: return clampIndoors(p.x, p.z - 0.9f);
        default: return clampIndoors(p.x, p.z + 0.9f);  // the chimney face
    }
}

TargetKind targetFor(Surface s, int32_t& index) {
    switch (s) {
        case Surface::Floor: index = -1; return TargetKind::Floor;
        case Surface::BackWall: index = 0; return TargetKind::Wall;
        case Surface::LeftWall: index = 1; return TargetKind::Wall;
        case Surface::RightWall: index = 2; return TargetKind::Wall;
        case Surface::FrontWall: index = 3; return TargetKind::Wall;
        default: index = -1; return TargetKind::HearthFace;
    }
}

int32_t itemIndex(const World& w, ItemKind k, ItemPlace place) {
    for (size_t i = 0; i < w.items.size(); ++i)
        if (w.items[i].kind == k && w.items[i].place == place) return static_cast<int32_t>(i);
    return -1;
}

// A spot beside the wagon from which an item on its bed is in reach: its
// front or one of its sides (the back is out of the way, beyond the wagon).
Vec2 standAtWagon(Vec3 p) {
    const float x0 = 1.2f, x1 = 3.8f, z0 = 7.0f, out = 0.45f;
    const float dl = p.x - x0, dr = x1 - p.x, df = p.z - z0;
    if (dl <= dr && dl <= df) return {x0 - out, p.z};
    if (dr <= df) return {x1 + out, p.z};
    return {p.x, z0 - out};
}

// Where each furnishing goes, as the point to aim at and where to stand.
struct Placement {
    TargetKind kind;
    int32_t index;
    Vec3 point;
    Vec2 stand;
};

Placement placementOf(ItemKind k) {
    const float mantel = L::kMantelY + 0.1f, face = L::kBreastFront;
    switch (k) {
        case ItemKind::Rug: return {TargetKind::Floor, -1, {-0.75f, L::kFloorY, -1.0f}, {-0.75f, 0.0f}};
        case ItemKind::Candle: return {TargetKind::HearthFace, -1, {0.5f, mantel, face}, {0.5f, -1.4f}};
        case ItemKind::Clock: return {TargetKind::HearthFace, -1, {-0.05f, mantel, face}, {-0.05f, -1.4f}};
        case ItemKind::Crock: return {TargetKind::HearthFace, -1, {-2.2f, mantel, face}, {-2.2f, -1.4f}};
        case ItemKind::Jar: return {TargetKind::HearthFace, -1, {-1.85f, mantel, face}, {-1.85f, -1.4f}};
        case ItemKind::Curtains: return {TargetKind::Wall, 1, {L::kInnerX0, 1.8f, -0.3f}, {-3.2f, -0.3f}};
        case ItemKind::Quilt: return {TargetKind::Chair, -1, {1.65f, 0.9f, -0.9f}, {2.3f, -0.3f}};
        case ItemKind::Sampler: return {TargetKind::Wall, 0, {2.4f, 1.9f, L::kInnerZ0}, {2.4f, -2.0f}};
        case ItemKind::Basket: return {TargetKind::Floor, -1, {1.0f, L::kFloorY, -1.9f}, {1.0f, -0.9f}};
        default: return {TargetKind::None, -1, {}, {}};
    }
}

constexpr ItemKind kFurnishOrder[] = {ItemKind::Rug,     ItemKind::Candle, ItemKind::Curtains,
                                      ItemKind::Quilt,   ItemKind::Sampler, ItemKind::Clock,
                                      ItemKind::Crock,   ItemKind::Jar,    ItemKind::Basket};

const Vec3 kFireboxPoint{(L::kFireboxX0 + L::kFireboxX1) / 2, 0.3f, -2.6f};
const Vec2 kFireboxStand{(L::kFireboxX0 + L::kFireboxX1) / 2, -1.3f};
const Vec3 kBlockPoint{-1.8f, 0.6f, 7.0f};
const Vec2 kBlockStand{-1.8f, 6.2f};
const Vec3 kLogPilePoint{-3.0f, 0.3f, 7.85f};
const Vec2 kLogPileStand{-3.0f, 6.9f};
const Vec3 kSplitPoint{-1.05f, 0.1f, 6.45f};
const Vec2 kSplitStand{-1.05f, 5.6f};
const Vec3 kKindlingPoint{-2.3f, 0.05f, 6.5f};
const Vec2 kKindlingStand{-2.3f, 5.8f};
const Vec3 kWoodpilePoint{-3.1f, 0.3f, -2.4f};
const Vec2 kWoodpileStand{-3.0f, -1.5f};
const Vec2 kHaulStand{2.5f, 3.3f};  // inside, by the door: the debris is tossed out through it
const Vec3 kHayPoint{5.4f, 0.4f, 8.25f};
const Vec2 kHayStand{5.4f, 7.4f};
const Vec3 kHorseHeadPoint{L::kHorse.x, 1.5f, L::kHorse.z - 1.0f};
const Vec2 kHorseFrontStand{L::kHorse.x, L::kHorse.z - 1.75f};
const Vec3 kHorseSidePoint{L::kHorse.x - 0.32f, 1.2f, L::kHorse.z};
const Vec2 kHorseSideStand{6.65f, L::kHorse.z};

}  // namespace

Autopilot::Goal Autopilot::choose(const World& w) {
    Goal g;
    auto go = [&](Vec2 stand, TargetKind kind, int32_t index, Vec3 point, Tool tool, bool use, bool press,
                  const char* what) {
        g.x = stand.x;
        g.z = stand.z;
        g.aim = aimAt(w, kind, index, point);
        g.tool = tool;
        g.use = use;
        g.press = press;
        activity_ = what;
        return g;
    };

    // ---- hands full: take it where it goes
    if (w.carry == Carry::Debris) return go(kHaulStand, TargetKind::None, -1, {2.5f, 0.5f, 7.0f}, Tool::None, false, true, "tossing debris out the door");
    if (w.carry == Carry::Item) {
        const Item& it = w.items[static_cast<size_t>(w.carryIndex)];
        const Vec3 wagon{2.5f, 0.86f, 7.1f};
        if (it.kind == ItemKind::Plank) {
            for (int i = 0; i < 3; ++i)
                if (w.planks[static_cast<size_t>(i)] == PlankState::Removed) {
                    const L::RottenSpan& s = L::kRottenSpans[static_cast<size_t>(i)];
                    const Vec3 p{(s.x0 + s.x1) / 2, L::kFloorY, L::plankZ0(s.row) + L::kPlankWidth / 2};
                    return go(clampIndoors(p.x, p.z + 1.0f), TargetKind::RottenPlank, i, p, Tool::None, true, false, "laying a new floorboard");
                }
            return go({2.5f, 6.55f}, TargetKind::Wagon, -1, wagon, Tool::None, false, true, "putting a plank back");
        }
        if (it.kind == ItemKind::Shingles) {
            const float xs[3] = {L::kRoofHoleX0, (L::kRoofHoleX0 + L::kRoofHoleX1) / 2, L::kRoofHoleX1};
            const float zs[3] = {L::kRoofHoleZ0, (L::kRoofHoleZ0 + L::kRoofHoleZ1) / 2, L::kRoofHoleZ1};
            for (int k = 0; k < 4; ++k)
                if (!((w.roofPatched >> k) & 1u)) {
                    const float cx = (xs[k % 2] + xs[k % 2 + 1]) / 2, cz = (zs[k / 2] + zs[k / 2 + 1]) / 2;
                    return go({cx, cz + 0.6f}, TargetKind::RoofHole, k, {cx, L::kCeilingY, cz}, Tool::None, true, false, "patching the roof");
                }
            return go({2.5f, 6.55f}, TargetKind::Wagon, -1, wagon, Tool::None, false, true, "putting shingles back");
        }
        const Placement pl = placementOf(it.kind);
        Pose pose;
        const Aim probe = aimAt(w, pl.kind, pl.index, pl.point);
        Aim reachable = probe;
        reachable.distance = 1.0f;  // judge the spot, not where we happen to be standing
        if (pl.kind == TargetKind::None || !placementFor(w, it.kind, reachable, it.yaw, pose))
            return go({2.5f, 6.55f}, TargetKind::Wagon, -1, wagon, Tool::None, false, true, "putting something back for later");
        return go(pl.stand, pl.kind, pl.index, pl.point, Tool::None, false, true, "placing a furnishing");
    }
    if (w.carry == Carry::Hay)
        return go(kHorseFrontStand, TargetKind::Horse, -1, kHorseHeadPoint, Tool::None, false, true, "feeding the mare");
    if (w.carry == Carry::Firewood) {
        const Wood k = static_cast<Wood>(w.carryIndex);
        const Hearth& h = w.hearth;
        if (k == Wood::Round)
            return w.wood.roundOnBlock ? go(kLogPileStand, TargetKind::LogPile, -1, kLogPilePoint, Tool::None, false, true, "returning a round")
                                       : go(kBlockStand, TargetKind::ChoppingBlock, -1, kBlockPoint, Tool::None, false, true, "setting a round on the block");
        if (k == Wood::Kindling)
            return go(kFireboxStand, TargetKind::Firebox, -1, kFireboxPoint, Tool::None, false, true, "laying kindling");
        const bool hearthWantsLog = h.ash <= 0 && (h.lit ? h.fuelMinutes < 120 : h.logsLaid == 0);
        if (hearthWantsLog) return go(kFireboxStand, TargetKind::Firebox, -1, kFireboxPoint, Tool::None, false, true, "laying a log on the fire");
        return go(kWoodpileStand, TargetKind::Woodpile, -1, kWoodpilePoint, Tool::None, false, true, "stacking firewood");
    }

    const Progress p = progress(w);
    auto haulNearest = [&]() -> bool {
        float best = 1e9f;
        int32_t pick = -1;
        for (size_t i = 0; i < w.debris.size(); ++i) {
            const Debris& d = w.debris[i];
            if (d.hauled || d.kind == DebrisKind::Rafter) continue;
            const float dx = d.pos.x - w.player.x, dz = d.pos.z - w.player.z, dist = dx * dx + dz * dz;
            if (dist < best) {
                best = dist;
                pick = static_cast<int32_t>(i);
            }
        }
        if (pick < 0) return false;
        const Debris& d = w.debris[static_cast<size_t>(pick)];
        go(clampIndoors(d.pos.x, d.pos.z + 0.8f), TargetKind::Debris, pick, d.pos, Tool::None, false, true, "picking up debris");
        return true;
    };

    switch (p.step) {
        case Step::Survey: {
            const Vec2 spot{-1.0f, 0.8f};
            const struct {
                uint32_t bit;
                TargetKind kind;
                Vec3 point;
            } looks[] = {{kSurveyGrime, TargetKind::Floor, {1.0f, L::kFloorY, 1.0f}},
                         {kSurveyDebris, TargetKind::Debris, w.debris[0].pos},
                         {kSurveyRottenPlank, TargetKind::RottenPlank, {0.75f, L::kFloorY, 0.7f}},
                         {kSurveyRoofHole, TargetKind::RoofHole, {0.1f, L::kCeilingY, -0.9f}},
                         {kSurveyBoardedWindow, TargetKind::WindowBoard, {L::kInnerX0, 1.5f, -0.3f}}};
            for (const auto& l : looks)
                if (!(w.surveyed & l.bit)) return go(spot, l.kind, 0, l.point, Tool::None, false, false, "surveying the damage");
            return go(spot, TargetKind::None, -1, {0, 1.6f, 0}, Tool::None, false, false, "surveying the damage");
        }
        case Step::Clear: {
            for (size_t i = 0; i < w.debris.size(); ++i) {
                const Debris& d = w.debris[i];
                if (!d.hauled && d.kind == DebrisKind::Rafter)
                    return go(clampIndoors(d.pos.x + 0.2f, d.pos.z + 0.9f), TargetKind::Debris, static_cast<int32_t>(i),
                              d.pos, Tool::Saw, true, false, "sawing the rafter");
            }
            if (haulNearest()) return g;
            if (w.snow > 0)
                return go({0.1f, 0.2f}, TargetKind::Snow, -1, {0.1f, 0.15f, -0.9f}, Tool::Shovel, true, false, "shovelling snow");
            // Grime: the nearest dirty patch on a surface not yet clean enough.
            const float goals[kSurfaceCount] = {kFloorCleanGoal + 0.05f, kWallCleanGoal + 0.05f, kWallCleanGoal + 0.05f,
                                                kWallCleanGoal + 0.05f, kWallCleanGoal + 0.05f, kWallCleanGoal + 0.05f};
            float best = 1e9f;
            int bs = -1, bi = 0, bj = 0;
            for (int s = 0; s < kSurfaceCount; ++s) {
                const Surface surf = static_cast<Surface>(s);
                if (cleanFraction(w.grime, surf) >= goals[s]) continue;
                const SurfaceDef& d = surfaceDef(surf);
                for (int j = 0; j < d.height; ++j)
                    for (int i = 0; i < d.width; ++i) {
                        if (w.grime.at(surf, i, j) <= kCleanThreshold) continue;
                        const int64_t key = static_cast<int64_t>(s) * 1000000 + j * 1000 + i;
                        if (skipped_.count(key)) continue;
                        const Vec2 st = standFor(surf, texelPoint(surf, i, j));
                        const float dx = st.x - w.player.x, dz = st.z - w.player.z, dist = dx * dx + dz * dz;
                        // Stay on one texel until it is clean, so the broom finishes its patch.
                        const float bias = key == targetKey_ ? -100.0f : 0.0f;
                        if (dist + bias < best) {
                            best = dist + bias;
                            bs = s;
                            bi = i;
                            bj = j;
                        }
                    }
            }
            if (bs >= 0) {
                const Surface surf = static_cast<Surface>(bs);
                const int64_t key = static_cast<int64_t>(bs) * 1000000 + bj * 1000 + bi;
                if (key != targetKey_) {
                    targetKey_ = key;
                    targetTicks_ = 0;
                }
                if (++targetTicks_ > kTexelPatience + 6 * kTickRate) {  // walking time allowed on top
                    skipped_.insert(key);
                    targetKey_ = -1;
                }
                int32_t index;
                const TargetKind kind = targetFor(surf, index);
                const Vec3 pt = texelPoint(surf, bi, bj);
                return go(standFor(surf, pt), kind, index, pt, surf == Surface::Floor ? Tool::Broom : Tool::ScrubBrush,
                          true, false, surf == Surface::Floor ? "sweeping the floor" : "scrubbing");
            }
            return go({0.5f, 1.0f}, TargetKind::None, -1, {0, 1.6f, -1}, Tool::None, false, false, "looking it over");
        }
        case Step::Repair: {
            const float boardY[4] = {1.14f, 1.44f, 1.75f, 2.03f};
            for (int b = 0; b < 4; ++b)
                if (w.windowBoards[static_cast<size_t>(b)])
                    return go({-3.2f, -0.27f}, TargetKind::WindowBoard, b, {L::kInnerX0, boardY[b], -0.27f}, Tool::PryBar,
                              true, false, "prying boards off the window");
            for (int i = 0; i < 3; ++i)
                if (w.planks[static_cast<size_t>(i)] == PlankState::Rotten) {
                    const L::RottenSpan& s = L::kRottenSpans[static_cast<size_t>(i)];
                    const Vec3 pt{(s.x0 + s.x1) / 2, L::kFloorY, L::plankZ0(s.row) + L::kPlankWidth / 2};
                    return go(clampIndoors(pt.x, pt.z + 1.0f), TargetKind::RottenPlank, i, pt, Tool::PryBar, true, false,
                              "prying up a rotten board");
                }
            if (haulNearest()) return g;
            for (int i = 0; i < 3; ++i)
                if (w.planks[static_cast<size_t>(i)] == PlankState::Removed) {
                    const int32_t it = itemIndex(w, ItemKind::Plank, ItemPlace::Wagon);
                    if (it >= 0) {
                        const Vec3 pt = w.items[static_cast<size_t>(it)].pos;
                        return go(standAtWagon(pt), TargetKind::Item, it, pt, Tool::None, false, true, "fetching a plank");
                    }
                }
            if (w.roofPatched != kRoofAllPatched) {
                const int32_t it = itemIndex(w, ItemKind::Shingles, ItemPlace::Wagon);
                if (it >= 0) {
                    const Vec3 pt = w.items[static_cast<size_t>(it)].pos;
                    return go(standAtWagon(pt), TargetKind::Item, it, pt, Tool::None, false, true, "fetching shingles");
                }
            }
            if (w.chair == ChairState::Broken)
                return go({1.65f, -0.2f}, TargetKind::Chair, -1, {1.65f, 0.6f, -1.15f}, Tool::Hammer, true, false, "mending the chair");
            if (w.table == TableState::Toppled)
                return go({-2.7f, 1.6f}, TargetKind::Table, -1, {-3.4f, 0.4f, 1.6f}, Tool::None, false, true, "righting the table");
            return go({0.5f, 1.0f}, TargetKind::None, -1, {0, 1.6f, -1}, Tool::None, false, false, "checking the repairs");
        }
        case Step::Furnish:
        case Step::Kindle: {
            // Furnish everything (more than the goal: the brief wants most of it).
            for (ItemKind k : kFurnishOrder) {
                const int32_t it = itemIndex(w, k, ItemPlace::Wagon);
                if (it < 0) continue;
                Pose pose;
                const Placement pl = placementOf(k);
                Aim probe = aimAt(w, pl.kind, pl.index, pl.point);
                probe.distance = 1.0f;
                if (!placementFor(w, k, probe, 0, pose)) continue;
                const Vec3 pt = w.items[static_cast<size_t>(it)].pos;
                return go(standAtWagon(pt), TargetKind::Item, it, pt, Tool::None, false, true, "fetching furnishings");
            }
            if (p.step == Step::Furnish)
                return go({0.5f, 1.0f}, TargetKind::None, -1, {0, 1.6f, -1}, Tool::None, false, false, "admiring the room");
            // Kindle.
            const Hearth& h = w.hearth;
            const Firewood& f = w.wood;
            const bool needKindling = !h.kindlingLaid && !h.lit;
            if (f.roundOnBlock) return go(kBlockStand, TargetKind::ChoppingBlock, -1, kBlockPoint, Tool::Axe, true, false, "splitting firewood");
            if (f.splitByBlock > 0) return go(kSplitStand, TargetKind::SplitWood, -1, kSplitPoint, Tool::None, false, true, "gathering split logs");
            if (h.ash > 0) return go(kFireboxStand, TargetKind::Firebox, -1, kFireboxPoint, Tool::Shovel, true, false, "shovelling out the ash");
            if (needKindling && f.kindlingByBlock > 0)
                return go(kKindlingStand, TargetKind::Kindling, -1, kKindlingPoint, Tool::None, false, true, "fetching kindling");
            if ((needKindling || f.woodpile < kWoodpileGoal + 1) && f.rounds > 0)
                return go(kLogPileStand, TargetKind::LogPile, -1, kLogPilePoint, Tool::None, false, true, "fetching a log round");
            if (!h.lit && h.logsLaid == 0 && f.woodpile > 0)
                return go(kWoodpileStand, TargetKind::Woodpile, -1, kWoodpilePoint, Tool::None, false, true, "taking a log for the fire");
            if (!h.lit) return go(kFireboxStand, TargetKind::Firebox, -1, kFireboxPoint, Tool::Matches, true, false, "lighting the fire");
            const int32_t candle = itemIndex(w, ItemKind::Candle, ItemPlace::Placed);
            if (candle >= 0 && !h.candleLit) {
                const Vec3 pt = w.items[static_cast<size_t>(candle)].pos + Vec3{0, 0.12f, 0};
                return go({pt.x, -1.4f}, TargetKind::Item, candle, pt, Tool::Matches, true, false, "lighting the candle");
            }
            return go({-2.6f, 1.2f}, TargetKind::None, -1, {-0.8f, 0.6f, -2.4f}, Tool::None, false, false, "watching the fire catch");
        }
        case Step::Welcome:
            return go({-2.8f, 2.6f}, TargetKind::None, -1, {w.settler.x, 1.3f, w.settler.z}, Tool::None, false, false, "welcoming Ada");
        case Step::Done:
            // Ada is home: see to the mare.
            if (w.horse.hunger > 0.3f && w.horse.eatingSeconds <= 0)
                return go(kHayStand, TargetKind::HayBale, -1, kHayPoint, Tool::None, false, true, "fetching hay");
            if (w.horse.dirt > 0)
                return go(kHorseSideStand, TargetKind::Horse, -1, kHorseSidePoint, Tool::CurryComb, true, false, "brushing the mare");
            return go({6.3f, 6.5f}, TargetKind::Horse, -1, kHorseSidePoint, Tool::None, false, false, "with the mare");
    }
    return g;
}

Input Autopilot::steer(const World& w, const Goal& g) {
    Input in;
    in.yaw = w.player.yaw;
    in.pitch = w.player.pitch;
    in.selectTool = static_cast<int32_t>(g.tool);

    // Through the door when the spot is on the other side of the wall.
    Vec2 to{g.x, g.z};
    const bool here = inside(w.player.x, w.player.z), there = inside(g.x, g.z);
    if (here != there) {
        const Vec2 mine = here ? kDoorInside : kDoorOutside, theirs = here ? kDoorOutside : kDoorInside;
        const float nearDoor = std::fabs(w.player.x - 2.5f) < 0.35f && std::fabs(w.player.z - 4.3f) < 1.0f ? 1.0f : 0.0f;
        to = nearDoor > 0 ? theirs : mine;
    }
    // Into and out of the stall by its open side.
    auto inStall = [](float x, float z) { return x > 6.2f && x < 9.0f && z > 7.4f && z < 11.0f; };
    const Vec2 stallMouth{7.6f, 6.9f};
    if (inStall(to.x, to.z) != inStall(w.player.x, w.player.z)) {
        // Lined up with the opening, walk straight on; otherwise go round to it first.
        const bool lined = std::fabs(w.player.x - stallMouth.x) < 0.5f && std::fabs(to.x - stallMouth.x) < 0.5f;
        if (!lined) to = stallMouth;
    }
    const float dx = to.x - w.player.x, dz = to.z - w.player.z;
    const float dist = std::sqrt(dx * dx + dz * dz);
    const bool arrived = here == there && dist <= kArrive && to.x == g.x && to.z == g.z;

    if (!arrived) {
        in.yaw = detAtan2(dx, -dz);
        in.pitch = 0;
        in.forward = 1;
        in.sprint = dist > 4.0f;
        // Unstick: if walking has not moved us, sidestep for a moment.
        const float moved = std::fabs(w.player.x - lastX_) + std::fabs(w.player.z - lastZ_);
        stuckTicks_ = moved < 0.002f ? stuckTicks_ + 1 : 0;
        if (stuckTicks_ > kStuckTicks) {
            in.strafe = (w.tick / kTickRate) % 2 ? 1.0f : -1.0f;
            if (stuckTicks_ > 3 * kStuckTicks && targetKey_ >= 0) {
                skipped_.insert(targetKey_);
                targetKey_ = -1;
                stuckTicks_ = 0;
            }
        }
        lastX_ = w.player.x;
        lastZ_ = w.player.z;
        pressedLast_ = false;
        return in;
    }
    stuckTicks_ = 0;
    lastX_ = w.player.x;
    lastZ_ = w.player.z;

    // Face what we are working on.
    if (g.aim.kind != TargetKind::None || g.press) {
        const Vec3 e = eyeOf(w), d = g.aim.point - e;
        in.yaw = detAtan2(d.x, -d.z);
        in.pitch = detAtan2(d.y, std::sqrt(d.x * d.x + d.z * d.z));
    }
    in.aim = aimAt(w, g.aim.kind, g.aim.index, g.aim.point);
    in.use = g.use;
    // Press on alternate ticks, so a press the rules refuse is not hammered every tick.
    in.interact = g.press && !pressedLast_;
    pressedLast_ = in.interact;
    return in;
}

Input Autopilot::next(const World& w) {
    Input in = steer(w, choose(w));
    // Carry the lantern after dark, as a player would.
    const double t = timeOfDay(w);
    const bool dark = t > 17.0 / 24.0 || t < 7.0 / 24.0;
    in.toggleLantern = dark != w.lantern;
    return in;
}

}  // namespace kindling
