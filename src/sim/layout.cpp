#include "sim/layout.h"

namespace kindling::layout {

const std::vector<Box2>& solids() {
    static const std::vector<Box2> boxes = [] {
        const float t = kWallThickness;
        std::vector<Box2> b;
        b.push_back({kInnerX0 - t, kInnerZ0 - t, kInnerX1 + t, kInnerZ0});  // back wall
        b.push_back({kInnerX0 - t, kInnerZ0 - t, kInnerX0, kInnerZ1 + t});  // left wall
        b.push_back({kInnerX1, kInnerZ0 - t, kInnerX1 + t, kInnerZ1 + t});  // right wall
        b.push_back({kInnerX0 - t, kInnerZ1, kDoorX0, kInnerZ1 + t});       // front wall, left of the door
        b.push_back({kDoorX1, kInnerZ1, kInnerX1 + t, kInnerZ1 + t});       // front wall, right of the door
        b.push_back({kBreastX0, kBreastZ0, kBreastX1, kBreastFront});      // chimney breast
        b.push_back({1.2f, 7.0f, 3.8f, 9.4f});                             // the wagon
        b.push_back({-2.15f, 6.65f, -1.45f, 7.35f});                       // chopping block
        b.push_back({-3.5f, 7.4f, -2.5f, 8.3f});                           // pile of log rounds
        b.push_back({5.0f, 7.9f, 5.8f, 8.6f});                             // hay bale
        // The stall: three fenced sides, open toward the cabin (low z).
        b.push_back({6.0f, 11.0f, 9.2f, 11.2f});
        b.push_back({6.0f, 7.4f, 6.2f, 11.2f});
        b.push_back({9.0f, 7.4f, 9.2f, 11.2f});
        b.push_back({kHorse.x - 0.35f, kHorse.z - 0.95f, kHorse.x + 0.35f, kHorse.z + 1.0f});  // the mare
        return b;
    }();
    return boxes;
}

}  // namespace kindling::layout
