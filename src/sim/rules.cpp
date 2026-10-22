#include "sim/rules.h"

#include <cmath>
#include <string>

#include "sim/layout.h"
#include "sim/progress.h"

namespace kindling {

namespace L = kindling::layout;

namespace {

constexpr float kMaxPitch = 1.45f;  // a little under straight up or down

// Cleaning rates: grime removed per tick at the centre of the tool's reach.
constexpr int kBroomStrength = 16, kBrushStrength = 14;
constexpr float kBroomRadius = 0.5f, kBrushRadius = 0.32f;
constexpr float kShovelPerSecond = 0.16f;  // share of the drift moved per second
constexpr float kSawSeconds = 4.0f;

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

// Pressing interact: pick something up, or put down what is carried.
void interact(World& w, const Input& in, TickEvents* ev) {
    if (w.carry == Carry::Debris) {
        Debris& d = w.debris[static_cast<size_t>(w.carryIndex)];
        if (!L::kInterior.contains(w.player.x, w.player.z)) {
            d.hauled = true;
            ++w.debrisHauled;
            say(ev, std::string("Hauled out ") + debrisName(d.kind) + ". " + std::to_string(piecesLeft(w)) +
                        " pieces left inside.");
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
    say(ev, "Sawn into three lengths. Haul them out.");
}

// Holding the primary action: work on the target with the tool in hand.
void work(World& w, const Input& in, TickEvents* ev) {
    const Aim& aim = in.aim;
    const bool sameJob = aim.kind == w.workKind && aim.index == w.workIndex;
    if (!in.use || w.carry != Carry::Nothing || !inReach(w, aim)) {
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

Vec3 carryPosition(const Player& p) {
    const Vec3 f = lookDirection(p.yaw, 0);
    const Vec3 right{detCos(p.yaw), 0, detSin(p.yaw)};
    return {p.x + f.x * 0.75f + right.x * 0.35f, L::kEyeHeight - 0.6f, p.z + f.z * 0.75f + right.z * 0.35f};
}

void tick(World& w, const Input& in, TickEvents* ev) {
    const Step before = progress(w).step;

    if (in.selectTool >= 0 && in.selectTool < kToolCount) w.tool = static_cast<Tool>(in.selectTool);
    move(w, in);
    if (in.interact) interact(w, in, ev);
    work(w, in, ev);

    if (w.carry == Carry::Debris) {
        Debris& d = w.debris[static_cast<size_t>(w.carryIndex)];
        d.pos = carryPosition(w.player);
        d.yaw = w.player.yaw + 1.2f;
    }

    if (in.aim.kind != TargetKind::None && in.aim.distance <= L::kReach * 2.5f) w.surveyed |= surveyBit(w, in.aim);

    const Step after = progress(w).step;
    if (after != before) {
        if (after == Step::Clear) say(ev, "You've seen the worst of it. Time to clear the place out.");
        if (after == Step::Repair) say(ev, "Cleared. Now for the repairs.");
    }
    ++w.tick;
}

}  // namespace kindling
