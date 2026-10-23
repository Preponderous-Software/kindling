#include "sim/rules.h"

#include <cmath>
#include <string>

#include "sim/brief.h"
#include "sim/layout.h"
#include "sim/progress.h"
#include "sim/settler.h"

namespace kindling {

namespace L = kindling::layout;

namespace {

constexpr float kMaxPitch = 1.45f;  // a little under straight up or down

// Cleaning rates: grime removed per tick at the centre of the tool's reach.
constexpr int kBroomStrength = 26, kBrushStrength = 26;
constexpr float kBroomRadius = 0.75f, kBrushRadius = 0.6f;
constexpr float kShovelPerSecond = 0.16f;  // share of the drift moved per second
constexpr float kSawSeconds = 4.0f;
constexpr float kPryPlankSeconds = 2.5f, kPryBoardSeconds = 2.0f;
constexpr float kLayPlankSeconds = 2.0f, kPatchSeconds = 2.5f, kMendSeconds = 3.0f;
constexpr float kRotateStep = 0.2617994f;  // 15 degrees
constexpr float kSplitSeconds = 1.5f, kAshPerSecond = 0.3f, kShieldSeconds = 4.0f, kCandleSeconds = 1.0f;
constexpr float kFuelPerLog = 240.0f;      // game minutes a split log burns
constexpr float kFireGrowthPerSecond = 0.035f;
constexpr int32_t kWoodpileMax = 14, kLogsOnAndirons = 3;
constexpr float kBrushPerSecond = 0.07f, kEatingSeconds = 12.0f, kHungerPerGameMinute = 1.0f / 360.0f;
constexpr float kGreetDistance = 3.5f;
constexpr float kCarryBelowView = 0.5f;  // radians below the crosshair a carried thing rides

// Pushes a circle out of one box, if it overlaps it.
void pushOut(float& x, float& z, float r, const Box2& b) {
    const float cx = clampf(x, b.x0, b.x1), cz = clampf(z, b.z0, b.z1);
    const float dx = x - cx, dz = z - cz;
    const float d2 = dx * dx + dz * dz;
    if (d2 >= r * r) return;
    if (d2 > 1e-12f) {
        const float d = std::sqrt(d2);
        x = cx + dx / d * r;
        z = cz + dz / d * r;
        return;
    }
    // The centre is inside the box: leave by the nearest side.
    const float left = x - b.x0, right = b.x1 - x, back = z - b.z0, front = b.z1 - z;
    if (left <= right && left <= back && left <= front) x = b.x0 - r;
    else if (right <= back && right <= front) x = b.x1 + r;
    else if (back <= front) z = b.z0 - r;
    else z = b.z1 + r;
}

void say(TickEvents* ev, std::string message) {
    if (ev) ev->messages.push_back(std::move(message));
}

void move(World& w, const Input& in) {
    Player& p = w.player;
    p.yaw = in.yaw - 2 * kPi * std::nearbyint(in.yaw / (2 * kPi));
    p.pitch = clampf(in.pitch, -kMaxPitch, kMaxPitch);

    float fwd = clampf(in.forward, -1, 1), side = clampf(in.strafe, -1, 1);
    const float len = std::sqrt(fwd * fwd + side * side);
    if (len > 1) {
        fwd /= len;
        side /= len;
    }
    const float speed = (in.sprint ? L::kSprintSpeed : L::kWalkSpeed) * kTickSeconds;
    const float sy = detSin(p.yaw), cy = detCos(p.yaw);
    // Forward is (sin yaw, -cos yaw); right is (cos yaw, sin yaw).
    const float dx = (fwd * sy + side * cy) * speed, dz = (-fwd * cy + side * sy) * speed;
    moveCircle(p.x, p.z, dx, dz, L::kPlayerRadius);
}

bool validDebris(const World& w, int32_t i) {
    return i >= 0 && static_cast<size_t>(i) < w.debris.size() && !w.debris[static_cast<size_t>(i)].hauled;
}

int32_t piecesLeft(const World& w) {
    int32_t n = 0;
    for (const Debris& d : w.debris) n += d.hauled ? 0 : 1;
    return n;
}

const char* debrisName(DebrisKind k) {
    switch (k) {
        case DebrisKind::Board: return "a broken board";
        case DebrisKind::Shingle: return "a fallen shingle";
        case DebrisKind::Rafter: return "the rafter";
        case DebrisKind::RafterPiece: return "a length of rafter";
    }
    return "debris";
}

bool validItem(const World& w, int32_t i) { return i >= 0 && static_cast<size_t>(i) < w.items.size(); }

// Old wood that comes away during repairs, dropped where it came from.
void spawnDebris(World& w, DebrisKind kind, Vec3 pos, float yaw) {
    int32_t nextId = 0;
    for (const Debris& d : w.debris) nextId = d.id >= nextId ? d.id + 1 : nextId;
    Debris d;
    d.id = nextId;
    d.kind = kind;
    d.pos = pos;
    d.yaw = yaw;
    w.debris.push_back(d);
}

// Putting down a carried item: back on the wagon, or where it belongs.
void putDownItem(World& w, const Input& in, TickEvents* ev) {
    Item& it = w.items[static_cast<size_t>(w.carryIndex)];
    if (in.aim.kind == TargetKind::Wagon && inReach(w, in.aim)) {
        const std::vector<Item> load = newItems();
        for (size_t i = 0; i < w.items.size(); ++i)
            if (w.items[i].place == ItemPlace::Carried) {
                w.items[i].place = ItemPlace::Wagon;
                w.items[i].pos = load[i].pos;
                w.items[i].yaw = load[i].yaw;
            }
    } else if (isMaterial(it.kind)) {
        say(ev, it.kind == ItemKind::Plank ? "Hold the plank over a gap in the floor and work it in (left mouse)."
                                           : "Stand under the hole in the roof and work the shingles in (left mouse).");
        return;
    } else {
        Pose pose;
        if (!placementFor(w, it.kind, in.aim, it.yaw, pose)) {
            say(ev, anchorHint(anchorOf(it.kind)));
            return;
        }
        const bool wasMet = evaluateBrief(w).met();
        it.place = ItemPlace::Placed;
        it.pos = pose.pos;
        it.yaw = pose.yaw;
        if (!wasMet && evaluateBrief(w).met()) say(ev, "Everything Ada asked for. She is going to love it.");
    }
    w.carry = Carry::Nothing;
    w.carryIndex = -1;
}

void carryWood(World& w, Wood k) {
    w.carry = Carry::Firewood;
    w.carryIndex = static_cast<int32_t>(k);
}

// Putting firewood where it goes: the block, the woodpile, the hearth.
void putDownWood(World& w, const Input& in, TickEvents* ev) {
    const Wood k = static_cast<Wood>(w.carryIndex);
    const TargetKind at = inReach(w, in.aim) ? in.aim.kind : TargetKind::None;
    Hearth& h = w.hearth;
    bool done = false;
    if (k == Wood::Round && at == TargetKind::ChoppingBlock && !w.wood.roundOnBlock) {
        w.wood.roundOnBlock = true;
        done = true;
    } else if (k == Wood::Round && at == TargetKind::LogPile) {
        ++w.wood.rounds;
        done = true;
    } else if (k == Wood::Split && at == TargetKind::Woodpile) {
        if (w.wood.woodpile >= kWoodpileMax) {
            say(ev, "The woodpile is full.");
            return;
        }
        ++w.wood.woodpile;
        done = true;
    } else if (k == Wood::Split && at == TargetKind::SplitWood) {
        ++w.wood.splitByBlock;
        done = true;
    } else if (k == Wood::Kindling && at == TargetKind::Kindling) {
        ++w.wood.kindlingByBlock;
        done = true;
    } else if (at == TargetKind::Firebox) {
        if (h.ash > 0) {
            say(ev, "Shovel the old ash out first (3).");
            return;
        }
        if (k == Wood::Kindling && !h.kindlingLaid && !h.lit) {
            h.kindlingLaid = true;
            say(ev, "Kindling laid. Now a split log or two on the andirons.");
            done = true;
        } else if (k == Wood::Split && h.lit) {
            h.fuelMinutes += kFuelPerLog;
            say(ev, "Another log on the fire.");
            done = true;
        } else if (k == Wood::Split && h.logsLaid < kLogsOnAndirons) {
            ++h.logsLaid;
            say(ev, h.kindlingLaid ? "Laid. Strike a match (8) and hold it till it catches." : "Laid. It needs kindling under it.");
            done = true;
        } else if (k == Wood::Round) {
            say(ev, "Split it first, on the chopping block.");
            return;
        }
    }
    if (!done) {
        say(ev, k == Wood::Round ? "Set it on the chopping block (E) and split it with the axe (7)."
                                 : k == Wood::Split ? "Stack it on the woodpile by the hearth, or lay it in the hearth (E)."
                                                    : "Lay it in the hearth (E).");
        return;
    }
    w.carry = Carry::Nothing;
    w.carryIndex = -1;
}

// Pressing interact: pick something up, or put down what is carried.
void interact(World& w, const Input& in, TickEvents* ev) {
    if (w.carry == Carry::Hay) {
        const TargetKind at = inReach(w, in.aim) ? in.aim.kind : TargetKind::None;
        if (at == TargetKind::Horse) {
            w.horse.hunger = 0;
            w.horse.eatingSeconds = kEatingSeconds;
            say(ev, "She buries her nose in the hay and munches.");
        } else if (at != TargetKind::HayBale) {
            say(ev, "Give it to the mare (E), or put it back on the bale.");
            return;
        }
        w.carry = Carry::Nothing;
        w.carryIndex = -1;
        return;
    }
    if (w.carry == Carry::Firewood) {
        putDownWood(w, in, ev);
        return;
    }
    if (w.carry == Carry::Item) {
        putDownItem(w, in, ev);
        return;
    }
    if (w.carry == Carry::Debris) {
        Debris& d = w.debris[static_cast<size_t>(w.carryIndex)];
        const bool outside = !L::kInterior.contains(w.player.x, w.player.z);
        if (outside || byTheDoor(w)) {
            d.hauled = true;
            ++w.debrisHauled;
            const int32_t left = piecesLeft(w);
            say(ev, std::string(outside ? "Hauled out " : "Tossed out the door: ") + debrisName(d.kind) + ". " +
                        (left == 0 ? "That's the last of it." :
                         left == 1 ? "One piece left inside." : std::to_string(left) + " pieces left inside."));
        } else {
            // Set it down in front of the player, on the floor.
            const Vec3 f = lookDirection(w.player.yaw, 0);
            d.pos = {w.player.x + f.x * 0.8f, L::kFloorY + 0.03f, w.player.z + f.z * 0.8f};
            d.yaw = w.player.yaw;
        }
        w.carry = Carry::Nothing;
        w.carryIndex = -1;
        return;
    }
    if (in.aim.kind == TargetKind::Debris && validDebris(w, in.aim.index) && inReach(w, in.aim)) {
        const Debris& d = w.debris[static_cast<size_t>(in.aim.index)];
        if (d.kind == DebrisKind::Rafter) {
            say(ev, "The rafter is too long to carry. Saw it into pieces (4).");
            return;
        }
        w.carry = Carry::Debris;
        w.carryIndex = in.aim.index;
        return;
    }
    if (in.aim.kind == TargetKind::Item && validItem(w, in.aim.index) && inReach(w, in.aim)) {
        Item& it = w.items[static_cast<size_t>(in.aim.index)];
        if (it.place == ItemPlace::Wagon && !isMaterial(it.kind) && !w.repaired) {
            say(ev, "That's for later: clear and mend the cabin first.");
            return;
        }
        if (it.place == ItemPlace::Wagon || it.place == ItemPlace::Placed) {
            it.place = ItemPlace::Carried;
            w.carry = Carry::Item;
            w.carryIndex = in.aim.index;
            // Planks and shingles come off the wagon all at once.
            if (isMaterial(it.kind))
                for (Item& other : w.items)
                    if (other.kind == it.kind && other.place == ItemPlace::Wagon) other.place = ItemPlace::Carried;
        }
        return;
    }
    if (in.aim.kind == TargetKind::Table && w.table == TableState::Toppled && inReach(w, in.aim)) {
        w.table = TableState::Upright;
        say(ev, "You set the table back on its legs.");
        return;
    }
    if (!inReach(w, in.aim)) return;
    switch (in.aim.kind) {
        case TargetKind::HayBale:
            w.carry = Carry::Hay;
            w.carryIndex = 0;
            break;
        case TargetKind::LogPile:
            if (w.wood.rounds > 0) {
                --w.wood.rounds;
                carryWood(w, Wood::Round);
            }
            break;
        case TargetKind::SplitWood:
            if (w.wood.splitByBlock > 0) {
                --w.wood.splitByBlock;
                carryWood(w, Wood::Split);
            }
            break;
        case TargetKind::Kindling:
            if (w.wood.kindlingByBlock > 0) {
                --w.wood.kindlingByBlock;
                carryWood(w, Wood::Kindling);
            }
            break;
        case TargetKind::Woodpile:
            if (w.wood.woodpile > 0) {
                --w.wood.woodpile;
                carryWood(w, Wood::Split);
            }
            break;
        default: break;
    }
}

Surface wallSurface(int32_t index) {
    switch (index) {
        case 0: return Surface::BackWall;
        case 1: return Surface::LeftWall;
        case 2: return Surface::RightWall;
        default: return Surface::FrontWall;
    }
}

void clean(World& w, Surface s, Vec3 p, float radius, int strength, TickEvents* ev) {
    const TexelRect r = scrub(w.grime, s, p, radius, strength);
    if (ev && r.w) {
        TexelRect& acc = ev->grime[static_cast<size_t>(s)];
        acc.add(r.x, r.y);
        acc.add(r.x + r.w - 1, r.y + r.h - 1);
    }
}

// The rafter is sawn into three lengths lying where it fell.
void sawRafter(World& w, int32_t index, TickEvents* ev) {
    Debris rafter = w.debris[static_cast<size_t>(index)];
    w.debris[static_cast<size_t>(index)].hauled = true;  // gone as a whole
    int32_t nextId = 0;
    for (const Debris& d : w.debris) nextId = d.id >= nextId ? d.id + 1 : nextId;
    const float c = detCos(rafter.yaw), s = detSin(rafter.yaw);
    for (int k = 0; k < 3; ++k) {
        Debris piece;
        piece.id = nextId++;
        piece.kind = DebrisKind::RafterPiece;
        const float along = 0.35f + 0.75f * static_cast<float>(k);
        piece.pos = {rafter.pos.x + c * along, L::kFloorY + 0.11f, rafter.pos.z - s * along + 0.25f * (k % 2)};
        piece.yaw = rafter.yaw + 0.2f * static_cast<float>(k - 1);
        w.debris.push_back(piece);
    }
    say(ev, "Sawn into three lengths, each light enough to carry: three pieces to haul out instead of one.");
}

// Holding the primary action: work on the target with the tool in hand.
void work(World& w, const Input& in, TickEvents* ev) {
    const Aim& aim = in.aim;
    const bool sameJob = aim.kind == w.workKind && aim.index == w.workIndex;
    const bool carryingMaterial = w.carry == Carry::Item && isMaterial(w.items[static_cast<size_t>(w.carryIndex)].kind);
    if (!in.use || (w.carry != Carry::Nothing && !carryingMaterial) || !inReach(w, aim)) {
        // Letting go of a struck match before the kindling catches puts it out.
        if (w.tool == Tool::Matches && w.workKind == TargetKind::Firebox && w.workSeconds > 0.2f && !w.hearth.lit) {
            ++w.hearth.matchesStruck;
            say(ev, "The match goes out. Hold it to the kindling until it catches.");
        }
        w.workKind = TargetKind::None;
        w.workIndex = -1;
        w.workSeconds = 0;
        return;
    }
    if (!sameJob) {
        w.workKind = aim.kind;
        w.workIndex = aim.index;
        w.workSeconds = 0;
    }
    w.workSeconds += kTickSeconds;

    if (carryingMaterial) {
        Item& it = w.items[static_cast<size_t>(w.carryIndex)];
        bool done = false;
        if (it.kind == ItemKind::Plank && aim.kind == TargetKind::RottenPlank && aim.index >= 0 && aim.index < 3 &&
            w.planks[static_cast<size_t>(aim.index)] == PlankState::Removed && w.workSeconds >= kLayPlankSeconds) {
            w.planks[static_cast<size_t>(aim.index)] = PlankState::New;
            say(ev, "A new board, pale against the old. It will darken with the years.");
            done = true;
        }
        if (it.kind == ItemKind::Shingles && aim.kind == TargetKind::RoofHole && aim.index >= 0 && aim.index < 4 &&
            !((w.roofPatched >> aim.index) & 1u) && w.workSeconds >= kPatchSeconds) {
            w.roofPatched |= 1u << aim.index;
            say(ev, w.roofPatched == kRoofAllPatched ? "The roof is whole again." : "One more section patched.");
            done = true;
        }
        if (done) {
            it.place = ItemPlace::Used;
            w.carry = Carry::Nothing;
            w.carryIndex = -1;
            w.workSeconds = 0;
            for (size_t i = 0; i < w.items.size(); ++i)
                if (w.items[i].place == ItemPlace::Carried) {
                    w.carry = Carry::Item;
                    w.carryIndex = static_cast<int32_t>(i);
                    break;
                }
        }
        return;
    }

    switch (w.tool) {
        case Tool::Broom:
            if (aim.kind == TargetKind::Floor) clean(w, Surface::Floor, aim.point, kBroomRadius, kBroomStrength, ev);
            break;
        case Tool::ScrubBrush:
            if (aim.kind == TargetKind::Wall)
                clean(w, wallSurface(aim.index), aim.point, kBrushRadius, kBrushStrength, ev);
            else if (aim.kind == TargetKind::HearthFace)
                clean(w, Surface::HearthFace, aim.point, kBrushRadius, kBrushStrength, ev);
            break;
        case Tool::Shovel:
            if (aim.kind == TargetKind::Firebox && w.hearth.ash > 0) {
                w.hearth.ash -= kAshPerSecond * kTickSeconds;
                if (w.hearth.ash <= 0) {
                    w.hearth.ash = 0;
                    say(ev, "The firebox is clean.");
                }
            }
            if (aim.kind == TargetKind::Snow && w.snow > 0) {
                w.snow -= kShovelPerSecond * kTickSeconds;
                if (w.snow <= 0) {
                    w.snow = 0;
                    say(ev, "The snow is out. Patch that hole before the next fall.");
                }
            }
            break;
        case Tool::Saw:
            if (aim.kind == TargetKind::Debris && validDebris(w, aim.index) &&
                w.debris[static_cast<size_t>(aim.index)].kind == DebrisKind::Rafter && w.workSeconds >= kSawSeconds) {
                sawRafter(w, aim.index, ev);
                w.workSeconds = 0;
            }
            break;
        case Tool::PryBar:
            if (aim.kind == TargetKind::RottenPlank && aim.index >= 0 && aim.index < 3 &&
                w.planks[static_cast<size_t>(aim.index)] == PlankState::Rotten && w.workSeconds >= kPryPlankSeconds) {
                const L::RottenSpan& s = L::kRottenSpans[static_cast<size_t>(aim.index)];
                w.planks[static_cast<size_t>(aim.index)] = PlankState::Removed;
                spawnDebris(w, DebrisKind::Board, {(s.x0 + s.x1) / 2, L::kFloorY + 0.03f, L::plankZ0(s.row) + 0.6f}, 0.1f);
                say(ev, "The rotten board comes up. Haul it out, then lay a new plank.");
                w.workSeconds = 0;
            }
            if (aim.kind == TargetKind::WindowBoard && aim.index >= 0 && aim.index < 4 &&
                w.windowBoards[static_cast<size_t>(aim.index)] && w.workSeconds >= kPryBoardSeconds) {
                w.windowBoards[static_cast<size_t>(aim.index)] = 0;
                spawnDebris(w, DebrisKind::Board, {L::kInnerX0 + 0.5f, L::kFloorY + 0.03f, -0.3f + 0.25f * aim.index},
                            1.5708f);
                bool open = true;
                for (int32_t b : w.windowBoards) open = open && b == 0;
                say(ev, open ? "Daylight floods in." : "Light comes through the gap.");
                w.workSeconds = 0;
            }
            break;
        case Tool::CurryComb:
            if (aim.kind == TargetKind::Horse && w.horse.dirt > 0) {
                if (w.horse.brushedSeconds <= 0 && w.horse.dirt > 0.75f) say(ev, "She leans into the brush.");
                w.horse.brushedSeconds = 1.5f;
                w.horse.dirt -= kBrushPerSecond * kTickSeconds;
                if (w.horse.dirt <= 0) {
                    w.horse.dirt = 0;
                    say(ev, "Her coat gleams.");
                }
            }
            break;
        case Tool::Axe:
            if (aim.kind == TargetKind::ChoppingBlock && w.wood.roundOnBlock && w.workSeconds >= kSplitSeconds) {
                w.wood.roundOnBlock = false;
                w.wood.splitByBlock += 2;
                w.wood.kindlingByBlock += 1;
                say(ev, "Split clean in two, with kindling from the splinters.");
                w.workSeconds = 0;
            }
            break;
        case Tool::Matches: {
            Hearth& h = w.hearth;
            if (aim.kind == TargetKind::Firebox && !h.lit) {
                if (h.ash > 0 || !h.kindlingLaid || h.logsLaid == 0) {
                    if (w.workSeconds <= kTickSeconds)
                        say(ev, h.ash > 0 ? "Shovel the old ash out first (3)."
                                          : !h.kindlingLaid ? "Lay kindling first: split a round at the chopping block."
                                                            : "Lay a split log on the andirons first: split another at the chopping "
                                                              "block, and keep the stack.");
                    w.workSeconds = 0;
                    w.workKind = TargetKind::None;
                } else if (w.workSeconds >= kShieldSeconds) {
                    h.lit = true;
                    h.fire = 0.05f;
                    h.fuelMinutes = kFuelPerLog * static_cast<float>(h.logsLaid);
                    h.logsLaid = 0;
                    h.kindlingLaid = false;
                    ++h.matchesStruck;
                    say(ev, "It catches. Let it take hold.");
                    w.workSeconds = 0;
                    w.workKind = TargetKind::None;
                }
            }
            if (aim.kind == TargetKind::Item && aim.index >= 0 && static_cast<size_t>(aim.index) < w.items.size()) {
                const Item& it = w.items[static_cast<size_t>(aim.index)];
                if (it.kind == ItemKind::Candle && it.place == ItemPlace::Placed && !h.candleLit &&
                    w.workSeconds >= kCandleSeconds) {
                    h.candleLit = true;
                    ++h.matchesStruck;
                    say(ev, "The candle takes the flame.");
                    w.workSeconds = 0;
                }
            }
            break;
        }
        case Tool::Hammer:
            if (aim.kind == TargetKind::Chair && w.chair == ChairState::Broken && w.workSeconds >= kMendSeconds) {
                w.chair = ChairState::Mended;
                say(ev, "The rocking chair is sound again.");
                w.workSeconds = 0;
            }
            break;
        default: break;
    }
}

}  // namespace

void moveCircle(float& x, float& z, float dx, float dz, float r) {
    // Small steps so a fast move cannot pass through a thin wall.
    const float dist = std::sqrt(dx * dx + dz * dz);
    const int steps = 1 + static_cast<int>(dist / (r * 0.5f));
    for (int s = 0; s < steps; ++s) {
        x += dx / steps;
        z += dz / steps;
        for (int pass = 0; pass < 2; ++pass)
            for (const Box2& b : L::solids()) pushOut(x, z, r, b);
        x = clampf(x, L::kYardX0 + r, L::kYardX1 - r);
        z = clampf(z, L::kYardZ0 + r, L::kYardZ1 - r);
    }
}

bool inReach(const World&, const Aim& aim) { return aim.kind != TargetKind::None && aim.distance <= L::kReach; }

bool byTheDoor(const World& w) {
    const float dx = w.player.x - (L::kDoorX0 + L::kDoorX1) / 2, dz = w.player.z - L::kInnerZ1;
    return L::kInterior.contains(w.player.x, w.player.z) && dx * dx + dz * dz <= L::kTossRange * L::kTossRange;
}

namespace {

// The meter, near enough or not.
float meterAnyReach(const World& w, const Aim& aim) {
    if (aim.kind == TargetKind::None) return -1;
    // A job held for a while: how much of `seconds` it has had.
    auto timed = [&](float seconds) {
        const bool same = w.workKind == aim.kind && w.workIndex == aim.index;
        return same ? std::fmin(1.0f, w.workSeconds / seconds) : 0.0f;
    };
    const size_t i = static_cast<size_t>(aim.index);
    if (w.carry == Carry::Item) {
        const ItemKind k = w.items[static_cast<size_t>(w.carryIndex)].kind;
        if (k == ItemKind::Plank && aim.kind == TargetKind::RottenPlank && aim.index >= 0 && aim.index < 3 &&
            w.planks[i] == PlankState::Removed)
            return timed(kLayPlankSeconds);
        if (k == ItemKind::Shingles && aim.kind == TargetKind::RoofHole && aim.index >= 0 && aim.index < 4 &&
            !((w.roofPatched >> aim.index) & 1u))
            return timed(kPatchSeconds);
        return -1;
    }
    if (w.carry != Carry::Nothing) return -1;
    switch (w.tool) {
        // The middle of the tool's disc: its edge cleans too slowly to wait for.
        case Tool::Broom:
            return aim.kind == TargetKind::Floor ? patchClean(w.grime, Surface::Floor, aim.point, 0.6f * kBroomRadius) : -1;
        case Tool::ScrubBrush:
            if (aim.kind == TargetKind::Wall)
                return patchClean(w.grime, wallSurface(aim.index), aim.point, 0.6f * kBrushRadius);
            if (aim.kind == TargetKind::HearthFace)
                return patchClean(w.grime, Surface::HearthFace, aim.point, 0.6f * kBrushRadius);
            return -1;
        case Tool::Shovel:
            if (aim.kind == TargetKind::Snow && w.snow > 0) return 1 - w.snow;
            if (aim.kind == TargetKind::Firebox && w.hearth.ash > 0) return 1 - w.hearth.ash;
            return -1;
        case Tool::Saw:
            return aim.kind == TargetKind::Debris && validDebris(w, aim.index) &&
                           w.debris[i].kind == DebrisKind::Rafter
                       ? timed(kSawSeconds)
                       : -1;
        case Tool::PryBar:
            if (aim.kind == TargetKind::RottenPlank && aim.index >= 0 && aim.index < 3 && w.planks[i] == PlankState::Rotten)
                return timed(kPryPlankSeconds);
            if (aim.kind == TargetKind::WindowBoard && aim.index >= 0 && aim.index < 4 && w.windowBoards[i])
                return timed(kPryBoardSeconds);
            return -1;
        case Tool::Hammer:
            return aim.kind == TargetKind::Chair && w.chair == ChairState::Broken ? timed(kMendSeconds) : -1;
        case Tool::Axe:
            return aim.kind == TargetKind::ChoppingBlock && w.wood.roundOnBlock ? timed(kSplitSeconds) : -1;
        case Tool::Matches: {
            const Hearth& h = w.hearth;
            if (aim.kind == TargetKind::Firebox && !h.lit && h.ash <= 0 && h.kindlingLaid && h.logsLaid > 0)
                return timed(kShieldSeconds);
            if (aim.kind == TargetKind::Item && validItem(w, aim.index) && w.items[i].kind == ItemKind::Candle &&
                w.items[i].place == ItemPlace::Placed && !h.candleLit)
                return timed(kCandleSeconds);
            return -1;
        }
        case Tool::CurryComb:
            return aim.kind == TargetKind::Horse && w.horse.dirt > 0 ? 1 - w.horse.dirt / Horse{}.dirt : -1;
        default: return -1;
    }
}

}  // namespace

float workMeter(const World& w, const Aim& aim) { return inReach(w, aim) ? meterAnyReach(w, aim) : -1; }

bool toolApplies(const World& w, const Aim& aim) {
    const float m = meterAnyReach(w, aim);
    return m >= 0 && m < 1;
}

Vec3 carryPosition(const Player& p) {
    // Low and to the right of the crosshair wherever the view points, so what
    // is carried stays in sight without hiding what it is carried to.
    const Vec3 f = lookDirection(p.yaw, std::fmax(p.pitch - kCarryBelowView, -1.55f));
    const Vec3 right{detCos(p.yaw), 0, detSin(p.yaw)};
    return {p.x + f.x * 0.7f + right.x * 0.3f, std::fmax(L::kEyeHeight + f.y * 0.7f, L::kFloorY + 0.08f),
            p.z + f.z * 0.7f + right.z * 0.3f};
}

// The fire grows toward what its fuel can feed, and dies down as it runs out.
void burn(World& w, TickEvents* ev) {
    Hearth& h = w.hearth;
    if (!h.lit) return;
    // Once Ada is settled by the fire she keeps it fed.
    if (w.settler.state == SettlerState::Seated && h.fuelMinutes < 60) h.fuelMinutes += kFuelPerLog;
    h.fuelMinutes -= kGameMinutesPerSecond * kTickSeconds;
    if (h.fuelMinutes < 0) h.fuelMinutes = 0;
    const float target = h.fuelMinutes >= 30 ? 1.0f : h.fuelMinutes / 30.0f;
    const float step = kFireGrowthPerSecond * kTickSeconds;
    if (h.fire < target) h.fire = h.fire + step > target ? target : h.fire + step;
    else if (h.fire > target) h.fire = h.fire - step * 2 < target ? target : h.fire - step * 2;
    if (h.fuelMinutes <= 0 && h.fire <= 0.01f) {
        h.lit = false;
        h.fire = 0;
        say(ev, "The fire has burned down to coals. Lay more wood and kindling.");
    }
}

void tick(World& w, const Input& in, TickEvents* ev) {
    const Progress was = progress(w);
    const Step before = was.step;
    const bool frostBefore = frosted(w);
    const bool duskBefore = dusk(w);
    if (in.toggleLantern) w.lantern = !w.lantern;

    if (in.selectTool >= 0 && in.selectTool < kToolCount) w.tool = static_cast<Tool>(in.selectTool);
    move(w, in);
    if (in.interact) interact(w, in, ev);
    work(w, in, ev);

    if (w.carry == Carry::Debris) {
        Debris& d = w.debris[static_cast<size_t>(w.carryIndex)];
        d.pos = carryPosition(w.player);
        d.yaw = w.player.yaw + 1.2f;
    }
    burn(w, ev);
    updateSettler(w, ev);
    // The mare: she gets hungry slowly, finishes her hay, and greets you.
    Horse& h = w.horse;
    h.hunger = std::fmin(1.0f, h.hunger + kHungerPerGameMinute * kGameMinutesPerSecond * kTickSeconds);
    h.eatingSeconds = std::fmax(0.0f, h.eatingSeconds - kTickSeconds);
    h.brushedSeconds = std::fmax(0.0f, h.brushedSeconds - kTickSeconds);
    const float hx = w.player.x - L::kHorse.x, hz = w.player.z - L::kHorse.z;
    if (!h.greeted && hx * hx + hz * hz < kGreetDistance * kGreetDistance) {
        h.greeted = true;
        say(ev, h.hunger > 0.5f ? "The mare nickers at you, hopefully. She's hungry." : "The mare nickers at you.");
    }
    if (w.carry == Carry::Item) {
        Item& it = w.items[static_cast<size_t>(w.carryIndex)];
        it.yaw += kRotateStep * static_cast<float>(in.rotate);
        it.yaw -= 2 * kPi * std::nearbyint(it.yaw / (2 * kPi));
        it.pos = carryPosition(w.player);
        // The rest of a stack of planks or shingles, under the one in hand.
        float below = 0;
        for (size_t i = 0; i < w.items.size(); ++i)
            if (w.items[i].place == ItemPlace::Carried && static_cast<int32_t>(i) != w.carryIndex) {
                below += w.items[i].kind == ItemKind::Plank ? 0.055f : 0.14f;
                w.items[i].pos = {it.pos.x, it.pos.y - below, it.pos.z};
                w.items[i].yaw = it.yaw;
            }
    }

    if (in.aim.kind != TargetKind::None && in.aim.distance <= L::kNameRange) w.surveyed |= surveyBit(w, in.aim);

    const Progress now = progress(w);
    // Each Clear job says so when it is done (the snow says it itself).
    if (was.step == Step::Clear) {
        if (was.floorClean < kFloorCleanGoal && now.floorClean >= kFloorCleanGoal) say(ev, "The floor is swept.");
        if (was.wallsClean < kWallCleanGoal && now.wallsClean >= kWallCleanGoal) say(ev, "The walls are scrubbed.");
        if (was.hearthClean < kWallCleanGoal && now.hearthClean >= kWallCleanGoal)
            say(ev, "The chimney stones are clean.");
    }
    if (!was.burning && now.burning) say(ev, "The fire has taken hold.");
    if (now.surveyed && now.cleared && !w.cleared) w.cleared = true;
    if (w.cleared && now.repairedNow && !w.repaired) w.repaired = true;
    if (w.repaired && now.furnished >= kFurnishGoal && now.burning && now.woodStacked && !w.kindled) w.kindled = true;
    const Step after = progress(w).step;
    if (after != before) {
        if (after == Step::Clear) say(ev, "You've seen the worst of it. Time to clear the place out.");
        if (after == Step::Repair) say(ev, "Cleared. Now for the repairs.");
        if (after == Step::Furnish) say(ev, "Sound again. Now make it a home: bring things in from the wagon.");
        if (after == Step::Kindle) say(ev, "It's starting to look like Ada's place. Time to light the hearth.");
        if (after == Step::Welcome) say(ev, "The cabin is warm. Ada's wagon is coming up the trail.");
        if (after == Step::Done) say(ev, "Ada is home. (T plays back your restoration.)");
    }
    ++w.tick;
    if (after != before || w.tick % kKeyframeEveryTicks == 0) w.history.push_back(snapshot(w));
    if (!frostBefore && frosted(w)) say(ev, "The first frost is settling over the valley.");
    if (!duskBefore && dusk(w) && !w.lantern) say(ev, "The light is going. L lights your lantern.");
}

}  // namespace kindling
