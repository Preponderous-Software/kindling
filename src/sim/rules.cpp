#include "sim/rules.h"

#include <cmath>

#include "sim/layout.h"

namespace kindling {

namespace {

constexpr float kMaxPitch = 1.45f;  // a little under straight up or down

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

}  // namespace

void moveCircle(float& x, float& z, float dx, float dz, float r) {
    // Small steps so a fast move cannot pass through a thin wall.
    const float dist = std::sqrt(dx * dx + dz * dz);
    const int steps = 1 + static_cast<int>(dist / (r * 0.5f));
    for (int s = 0; s < steps; ++s) {
        x += dx / steps;
        z += dz / steps;
        for (int pass = 0; pass < 2; ++pass)
            for (const Box2& b : layout::solids()) pushOut(x, z, r, b);
        x = clampf(x, layout::kYardX0 + r, layout::kYardX1 - r);
        z = clampf(z, layout::kYardZ0 + r, layout::kYardZ1 - r);
    }
}

void tick(World& w, const Input& in) {
    Player& p = w.player;
    p.yaw = in.yaw - 2 * kPi * std::nearbyint(in.yaw / (2 * kPi));
    p.pitch = clampf(in.pitch, -kMaxPitch, kMaxPitch);

    float fwd = clampf(in.forward, -1, 1), side = clampf(in.strafe, -1, 1);
    const float len = std::sqrt(fwd * fwd + side * side);
    if (len > 1) {
        fwd /= len;
        side /= len;
    }
    const float speed = (in.sprint ? layout::kSprintSpeed : layout::kWalkSpeed) * kTickSeconds;
    const float sy = detSin(p.yaw), cy = detCos(p.yaw);
    // Forward is (sin yaw, -cos yaw); right is (cos yaw, sin yaw).
    const float dx = (fwd * sy + side * cy) * speed, dz = (-fwd * cy + side * sy) * speed;
    moveCircle(p.x, p.z, dx, dz, layout::kPlayerRadius);

    if (in.aim.kind != TargetKind::None && in.aim.distance <= layout::kReach * 2.5f)
        w.surveyed |= surveyBit(w, in.aim);

    ++w.tick;
}

}  // namespace kindling
