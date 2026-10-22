// Things the player carries from the wagon: building materials for the
// repairs, and the furnishings that make the cabin a home.
#pragma once

#include <cstdint>
#include <vector>

#include "sim/math.h"
#include "sim/target.h"

namespace kindling {

struct World;

enum class ItemKind : int32_t {
    // Materials, used up by repairs.
    Plank = 0,       // a new floorboard
    Shingles,        // a bundle that patches one section of the roof hole
    // Furnishings, placed where they belong.
    Rug,
    Quilt,
    Curtains,
    Sampler,
    Crock,
    Jar,
    Clock,
    Candle,
    Basket,
    Count
};
const char* itemName(ItemKind k);
bool isMaterial(ItemKind k);

enum class ItemPlace : int32_t { Wagon = 0, Carried = 1, Placed = 2, Used = 3 };

// Where a furnishing may go.
enum class Anchor : int32_t { None = 0, Floor, Mantel, Wall, Chair, Window };
Anchor anchorOf(ItemKind k);
const char* anchorHint(Anchor a);  // "A rug goes on the floor."

struct Item {
    int32_t id = 0;
    ItemKind kind = ItemKind::Plank;
    ItemPlace place = ItemPlace::Wagon;
    Vec3 pos;     // on the wagon, on the floor, on the mantel ...
    float yaw = 0;
};

// The wagon's load at the start: three planks, four bundles of shingles, and
// one of each furnishing, in fixed spots on the wagon bed.
std::vector<Item> newItems();

// Where the carried furnishing would go if put down now, given what the
// crosshair is on. False (and nothing written) when it cannot go there.
struct Pose {
    Vec3 pos;
    float yaw = 0;
};
bool placementFor(const World& w, ItemKind kind, const Aim& aim, float yaw, Pose& out);

}  // namespace kindling
