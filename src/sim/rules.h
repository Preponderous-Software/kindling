// The rules: one tick of the world from one tick of input.
//
// Deterministic: the same World and the same sequence of Inputs always give
// the same World, on every compiler. That is what the fingerprint tests pin,
// and what lets a recorded restoration be played back.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "sim/grime.h"
#include "sim/target.h"
#include "sim/world.h"

namespace kindling {

// One tick of player intent, built by the app from keys and mouse (or by a
// test or the autopilot).
struct Input {
    float forward = 0;  // -1..1, along the look direction on the ground
    float strafe = 0;   // -1..1, to the right
    bool sprint = false;
    float yaw = 0, pitch = 0;  // absolute look angles (radians); pitch is clamped
    Aim aim;                   // what the crosshair is on this tick
    bool use = false;          // the primary action, held (work with the tool in hand)
    bool interact = false;     // pick up, put down, set right: pressed this tick
    int32_t selectTool = -1;   // -1 keeps the tool in hand; otherwise a Tool value
};

// What a tick changed that the app needs to show: grime texels to re-upload,
// and messages for the player.
struct TickEvents {
    std::array<TexelRect, kSurfaceCount> grime{};
    std::vector<std::string> messages;
};

void tick(World& w, const Input& in, TickEvents* events = nullptr);

// Moves a circle of radius r from (x, z) by (dx, dz), sliding along anything
// solid and staying inside the yard. Exposed for tests.
void moveCircle(float& x, float& z, float dx, float dz, float r);

// How far from the player's eye things can be reached and worked on.
bool inReach(const World& w, const Aim& aim);

// Where carried things ride: low and to the right, clear of the crosshair.
Vec3 carryPosition(const Player& p);

}  // namespace kindling
