// The fixed shape of the MVP's world: one log cabin (the hearth room from
// prototypes/hearth-scene) and its yard. Coordinates are metres, y up; the
// back wall with the hearth is at -z and the door at +z.
//
// Both the simulation (collision, targets) and the render bridge (meshes) read
// these numbers, so the room you see is the room you bump into.
#pragma once

#include <array>
#include <vector>

#include "sim/math.h"

namespace kindling::layout {

// ---- the room
constexpr float kFloorY = 0.06f;    // top of the floorboards
constexpr float kCeilingY = 2.95f;  // underside of the ceiling boards
constexpr float kInnerX0 = -3.95f, kInnerX1 = 3.95f;  // inside faces of the side walls
constexpr float kInnerZ0 = -2.95f, kInnerZ1 = 4.13f;  // inside faces of the back and front walls
constexpr float kWallThickness = 0.34f;               // one log's diameter

// The door in the front wall.
constexpr float kDoorX0 = 2.1f, kDoorX1 = 2.9f, kDoorTop = 2.1f;

// The window in the left wall (x = kInnerX0).
constexpr float kWindowZ0 = -1.0f, kWindowZ1 = 0.45f, kWindowY0 = 1.0f, kWindowY1 = 2.1f;

// The chimney breast and its firebox.
constexpr float kBreastX0 = -2.3f, kBreastX1 = 0.7f, kBreastZ0 = -2.98f, kBreastFront = -2.35f;
constexpr float kFireboxX0 = -1.45f, kFireboxX1 = -0.15f, kFireboxTop = 1.05f;
constexpr float kHearthstoneZ1 = -1.72f;  // the hearthstone runs from the breast front to here
constexpr float kMantelY = 1.36f;         // top of the mantel shelf
constexpr float kMantelX0 = -2.6f, kMantelX1 = 1.0f, kMantelZ0 = -2.62f, kMantelZ1 = -2.08f;

// The hole where the roof fell in.
constexpr float kRoofHoleX0 = -0.6f, kRoofHoleX1 = 0.8f, kRoofHoleZ0 = -1.5f, kRoofHoleZ1 = -0.3f;

// ---- floorboards: kPlankRows rows running along x, from the back wall forward
constexpr int kPlankRows = 29;
constexpr float kPlankWidth = (kInnerZ1 - kInnerZ0) / kPlankRows;
inline float plankZ0(int row) { return kInnerZ0 + row * kPlankWidth; }

// A rotten stretch of one floorboard.
struct RottenSpan {
    int row;
    float x0, x1;
};
constexpr std::array<RottenSpan, 3> kRottenSpans{{{11, -1.9f, -1.0f}, {15, 0.3f, 1.2f}, {16, 0.6f, 1.5f}}};

// ---- the yard
constexpr float kYardX0 = -12.0f, kYardX1 = 14.0f, kYardZ0 = -8.0f, kYardZ1 = 18.0f;  // walkable bounds
constexpr Vec2 kSpawn{2.5f, 6.2f};  // the player starts in the yard, facing the door
constexpr float kSpawnYaw = 0.0f;   // facing -z, toward the cabin

// ---- the player
constexpr float kPlayerRadius = 0.3f;
constexpr float kEyeHeight = 1.62f;
constexpr float kWalkSpeed = 2.6f;    // m/s
constexpr float kSprintSpeed = 4.6f;  // m/s
constexpr float kReach = 2.2f;        // how far the player can work or pick things up
// The snow drift under the roof hole, at its full size: centre and radii.
constexpr float kSnowX = 0.1f, kSnowZ = -0.9f, kSnowRX = 0.8f, kSnowRZ = 0.65f;
constexpr float kNameRange = 10.5f;   // how far things are named (and surveyed): across the room, corner to corner
constexpr float kTossRange = 1.6f;    // debris carried this near the door can be tossed out through it

// The mare stands in her stall facing out, toward the cabin.
constexpr Vec2 kHorse{7.6f, 9.6f};

// Everything the player cannot walk through, on the ground plane: the log
// walls (with the door gap), the chimney breast, and the yard's fixtures.
const std::vector<Box2>& solids();

// The inside of the cabin, for "is the player indoors" checks.
constexpr Box2 kInterior{kInnerX0, kInnerZ0, kInnerX1, kInnerZ1};

}  // namespace kindling::layout
