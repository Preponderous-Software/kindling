// Walking: walls stop the player, the door lets them through, and moving into
// a wall slides along it instead of sticking.
#include <cmath>

#include "check.h"
#include "sim/layout.h"
#include "sim/rules.h"

using namespace kindling;
namespace L = kindling::layout;

namespace {

// Walk with a fixed look direction for n ticks.
void walk(World& w, float yaw, float forward, float strafe, int n) {
    Input in;
    in.yaw = yaw;
    in.forward = forward;
    in.strafe = strafe;
    for (int i = 0; i < n; ++i) tick(w, in);
}

bool inside(const World& w) { return L::kInterior.contains(w.player.x, w.player.z); }

}  // namespace

int main() {
    // The player starts in the yard, in front of the door.
    {
        World w = newWorld(1);
        CHECK(!inside(w));
        CHECK(w.player.z > L::kInnerZ1 + L::kWallThickness);
    }

    // Walking straight at the door (yaw 0 faces -z) takes the player inside.
    {
        World w = newWorld(1);
        walk(w, 0.0f, 1, 0, 3 * kTickRate);
        CHECK(inside(w));
    }

    // Walking at the front wall left of the door stops at the wall, outside.
    {
        World w = newWorld(1);
        w.player.x = 0.0f;
        walk(w, 0.0f, 1, 0, 5 * kTickRate);
        CHECK(!inside(w));
        CHECK(std::fabs(w.player.z - (L::kInnerZ1 + L::kWallThickness + L::kPlayerRadius)) < 0.01f);
    }

    // Inside, walking into the back wall stops a radius short of it, and the
    // chimney breast blocks the middle of the back wall.
    {
        World w = newWorld(1);
        w.player.x = 3.0f;
        w.player.z = 1.0f;
        walk(w, 0.0f, 1, 0, 6 * kTickRate);
        CHECK(std::fabs(w.player.z - (L::kInnerZ0 + L::kPlayerRadius)) < 0.01f);

        w.player.x = -0.8f;
        w.player.z = 1.0f;
        walk(w, 0.0f, 1, 0, 6 * kTickRate);
        CHECK(std::fabs(w.player.z - (L::kBreastFront + L::kPlayerRadius)) < 0.01f);
    }

    // Moving diagonally into a wall slides along it instead of sticking.
    {
        World w = newWorld(1);
        w.player.x = 0.0f;
        w.player.z = 3.5f;
        const float yaw = -1.0f;  // forward-left, mostly toward the left wall
        walk(w, yaw, 1, 0, 2 * kTickRate);
        const float xAtWall = w.player.x;
        CHECK(std::fabs(xAtWall - (L::kInnerX0 + L::kPlayerRadius)) < 0.01f);
        const float zBefore = w.player.z;
        walk(w, yaw, 1, 0, kTickRate);
        CHECK(w.player.z < zBefore - 0.5f);  // still moving toward the back wall
    }

    // A sprint covers more ground than a walk in the same time.
    {
        World a = newWorld(1), b = newWorld(1);
        a.player.x = b.player.x = 0;
        a.player.z = b.player.z = 12;
        Input walkIn, sprintIn;
        walkIn.forward = sprintIn.forward = 1;
        walkIn.yaw = sprintIn.yaw = 3.14159265f;  // facing +z, away from the cabin
        sprintIn.sprint = true;
        for (int i = 0; i < kTickRate; ++i) {
            tick(a, walkIn);
            tick(b, sprintIn);
        }
        CHECK(std::fabs((a.player.z - 12) - L::kWalkSpeed) < 0.05f);
        CHECK(std::fabs((b.player.z - 12) - L::kSprintSpeed) < 0.05f);
    }

    // Pitch is clamped short of straight up and down.
    {
        World w = newWorld(1);
        Input in;
        in.pitch = 3.0f;
        tick(w, in);
        CHECK(w.player.pitch < 1.5f);
    }

    TEST_MAIN_END();
}
