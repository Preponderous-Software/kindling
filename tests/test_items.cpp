// Items: the wagon's load, what each thing is called, where each furnishing
// belongs, and where one goes when it is put down at what the crosshair is on.
#include <set>
#include <string>

#include "check.h"
#include "sim/items.h"
#include "sim/layout.h"
#include "sim/world.h"

using namespace kindling;
namespace L = kindling::layout;

namespace {

Aim aimAt(TargetKind kind, int32_t index, Vec3 point, float distance = 1.0f) {
    Aim a;
    a.kind = kind;
    a.index = index;
    a.point = point;
    a.distance = distance;
    return a;
}

// A pose no placement ever writes, to see that a refused placement leaves it alone.
const Pose kUntouched{{-99.0f, -99.0f, -99.0f}, -9.0f};

bool untouched(const Pose& p) {
    return p.pos.x == kUntouched.pos.x && p.pos.y == kUntouched.pos.y && p.pos.z == kUntouched.pos.z &&
           p.yaw == kUntouched.yaw;
}

}  // namespace

int main() {
    // ---- names, materials and anchors
    std::set<std::string> names;
    for (int k = 0; k < static_cast<int>(ItemKind::Count); ++k) {
        const std::string name = itemName(static_cast<ItemKind>(k));
        CHECK(!name.empty());
        names.insert(name);
    }
    CHECK(names.size() == static_cast<size_t>(ItemKind::Count));  // no two things share a name
    CHECK(std::string(itemName(ItemKind::Count)).empty());

    CHECK(isMaterial(ItemKind::Plank));
    CHECK(isMaterial(ItemKind::Shingles));
    for (int k = static_cast<int>(ItemKind::Rug); k < static_cast<int>(ItemKind::Count); ++k)
        CHECK(!isMaterial(static_cast<ItemKind>(k)));

    // Materials are used up by repairs, never placed.
    CHECK(anchorOf(ItemKind::Plank) == Anchor::None);
    CHECK(anchorOf(ItemKind::Shingles) == Anchor::None);
    CHECK(anchorOf(ItemKind::Rug) == Anchor::Floor);
    CHECK(anchorOf(ItemKind::Basket) == Anchor::Floor);
    CHECK(anchorOf(ItemKind::Crock) == Anchor::Mantel);
    CHECK(anchorOf(ItemKind::Jar) == Anchor::Mantel);
    CHECK(anchorOf(ItemKind::Clock) == Anchor::Mantel);
    CHECK(anchorOf(ItemKind::Candle) == Anchor::Mantel);
    CHECK(anchorOf(ItemKind::Sampler) == Anchor::Wall);
    CHECK(anchorOf(ItemKind::Quilt) == Anchor::Chair);
    CHECK(anchorOf(ItemKind::Curtains) == Anchor::Window);

    CHECK(std::string(anchorHint(Anchor::None)).empty());
    const Anchor anchors[] = {Anchor::Floor, Anchor::Mantel, Anchor::Wall, Anchor::Chair, Anchor::Window};
    for (Anchor a : anchors) CHECK(!std::string(anchorHint(a)).empty());

    // ---- the wagon's load: three planks, four bundles of shingles, one of each furnishing
    {
        const std::vector<Item> load = newItems();
        int planks = 0, shingles = 0;
        std::set<int> furnishings, ids;
        for (const Item& it : load) {
            CHECK(it.place == ItemPlace::Wagon);
            ids.insert(it.id);
            if (it.kind == ItemKind::Plank) ++planks;
            else if (it.kind == ItemKind::Shingles) ++shingles;
            else furnishings.insert(static_cast<int>(it.kind));
        }
        CHECK(planks == 3);
        CHECK(shingles == 4);
        CHECK(furnishings.size() == static_cast<size_t>(ItemKind::Count) - 2);
        CHECK(load.size() == 3 + 4 + furnishings.size());
        CHECK(ids.size() == load.size());   // every id distinct
        CHECK(*ids.begin() == 1);           // numbered from 1
        CHECK(*ids.rbegin() == static_cast<int>(load.size()));
    }

    const World w = newWorld(7);
    const float yaw = 0.7f;

    // ---- nothing aimed at, out of reach, or a material: refused, the pose untouched
    {
        Pose p = kUntouched;
        CHECK(!placementFor(w, ItemKind::Rug, Aim{}, yaw, p));
        CHECK(!placementFor(w, ItemKind::Rug, aimAt(TargetKind::Floor, -1, {0, L::kFloorY, 1}, L::kReach + 0.1f), yaw, p));
        CHECK(!placementFor(w, ItemKind::Plank, aimAt(TargetKind::Floor, -1, {0, L::kFloorY, 1}), yaw, p));
        CHECK(untouched(p));
        // At exactly arm's length it still reaches.
        CHECK(placementFor(w, ItemKind::Rug, aimAt(TargetKind::Floor, -1, {0, L::kFloorY, 1}, L::kReach), yaw, p));
    }

    // ---- the floor: on the boards indoors, turned as the player turned it
    {
        Pose p = kUntouched;
        CHECK(placementFor(w, ItemKind::Rug, aimAt(TargetKind::Floor, -1, {1.0f, 0.2f, 2.0f}), yaw, p));
        CHECK(p.pos.x == 1.0f && p.pos.y == L::kFloorY && p.pos.z == 2.0f);
        CHECK(p.yaw == yaw);

        // Not in the yard, and not on a wall.
        p = kUntouched;
        CHECK(!placementFor(w, ItemKind::Basket, aimAt(TargetKind::Floor, -1, {1.0f, L::kFloorY, L::kInnerZ1 + 1}), yaw, p));
        CHECK(!placementFor(w, ItemKind::Basket, aimAt(TargetKind::Wall, 0, {1.0f, 1.0f, L::kInnerZ0}), yaw, p));
        CHECK(untouched(p));

        // A rotten board is no floor to put a rug on; a new one is.
        const Vec3 onPlank{0.5f, L::kFloorY, 1.0f};
        CHECK(w.planks[0] == PlankState::Rotten);
        CHECK(!placementFor(w, ItemKind::Rug, aimAt(TargetKind::RottenPlank, 0, onPlank), yaw, p));
        World laid = w;
        laid.planks[0] = PlankState::Removed;
        CHECK(!placementFor(laid, ItemKind::Rug, aimAt(TargetKind::RottenPlank, 0, onPlank), yaw, p));
        laid.planks[0] = PlankState::New;
        CHECK(placementFor(laid, ItemKind::Rug, aimAt(TargetKind::RottenPlank, 0, onPlank), yaw, p));
        CHECK(p.pos.y == L::kFloorY);
        // A board index past the last one is no board at all. The window boards sit
        // just after the planks, so one set to New's value would pass for a laid board
        // if the index were not checked.
        laid.windowBoards[0] = static_cast<int32_t>(PlankState::New);
        p = kUntouched;
        CHECK(!placementFor(laid, ItemKind::Rug, aimAt(TargetKind::RottenPlank, 3, onPlank), yaw, p));
        CHECK(untouched(p));
    }

    // ---- the mantel: on the shelf or the stones just above it, set square to the room
    {
        const float midZ = (L::kMantelZ0 + L::kMantelZ1) / 2;
        Pose p = kUntouched;
        CHECK(placementFor(w, ItemKind::Clock, aimAt(TargetKind::HearthFace, -1, {-0.8f, L::kMantelY + 0.3f, -2.3f}), yaw, p));
        CHECK(p.pos.x == -0.8f && p.pos.y == L::kMantelY && p.pos.z == midZ);
        CHECK(p.yaw == 0);

        p = kUntouched;
        // Off either end of the shelf.
        CHECK(!placementFor(w, ItemKind::Jar, aimAt(TargetKind::HearthFace, -1, {L::kMantelX0 + 0.05f, L::kMantelY, -2.3f}), yaw, p));
        CHECK(!placementFor(w, ItemKind::Jar, aimAt(TargetKind::HearthFace, -1, {L::kMantelX1 - 0.05f, L::kMantelY, -2.3f}), yaw, p));
        // Too far below or above it.
        CHECK(!placementFor(w, ItemKind::Candle, aimAt(TargetKind::HearthFace, -1, {-0.8f, L::kMantelY - 0.2f, -2.3f}), yaw, p));
        CHECK(!placementFor(w, ItemKind::Candle, aimAt(TargetKind::HearthFace, -1, {-0.8f, L::kMantelY + 0.6f, -2.3f}), yaw, p));
        // Only on the chimney's stones.
        CHECK(!placementFor(w, ItemKind::Crock, aimAt(TargetKind::Wall, 0, {-0.8f, L::kMantelY, L::kInnerZ0}), yaw, p));
        CHECK(untouched(p));
    }

    // ---- a wall: between 1.2 m and 2.5 m up, facing into the room
    {
        struct Facing {
            int32_t wall;
            Vec3 point;
            float yaw;
        };
        const Facing walls[4] = {
            {0, {2.4f, 1.6f, L::kInnerZ0}, 0.0f},
            {1, {L::kInnerX0, 1.6f, 2.0f}, 1.5708f},
            {2, {L::kInnerX1, 1.6f, 1.0f}, -1.5708f},
            {3, {-2.0f, 1.6f, L::kInnerZ1}, 3.14159f},
        };
        for (const Facing& f : walls) {
            Pose p = kUntouched;
            CHECK(placementFor(w, ItemKind::Sampler, aimAt(TargetKind::Wall, f.wall, f.point), yaw, p));
            CHECK(p.pos.x == f.point.x && p.pos.y == f.point.y && p.pos.z == f.point.z);
            CHECK(p.yaw == f.yaw);
        }

        Pose p = kUntouched;
        CHECK(!placementFor(w, ItemKind::Sampler, aimAt(TargetKind::Wall, 0, {2.4f, 1.1f, L::kInnerZ0}), yaw, p));
        CHECK(!placementFor(w, ItemKind::Sampler, aimAt(TargetKind::Wall, 0, {2.4f, 2.6f, L::kInnerZ0}), yaw, p));
        CHECK(!placementFor(w, ItemKind::Sampler, aimAt(TargetKind::Floor, -1, {2.4f, 1.6f, 1.0f}), yaw, p));
        // The chimney breast's side, where it would stand edge-on to the room.
        CHECK(!placementFor(w, ItemKind::Sampler, aimAt(TargetKind::HearthFace, -1, {L::kBreastX1, 1.8f, L::kBreastFront - 0.3f}), yaw, p));
        CHECK(untouched(p));
        // The breast's front faces the room like the back wall.
        CHECK(placementFor(w, ItemKind::Sampler, aimAt(TargetKind::HearthFace, -1, {-0.8f, 1.8f, L::kBreastFront}), yaw, p));
        CHECK(p.yaw == 0);
    }

    // ---- the rocking chair: only once it is mended
    {
        const Aim chair = aimAt(TargetKind::Chair, -1, {1.6f, 0.8f, -0.8f});
        Pose p = kUntouched;
        CHECK(w.chair == ChairState::Broken);
        CHECK(!placementFor(w, ItemKind::Quilt, chair, yaw, p));
        CHECK(untouched(p));
        World mended = w;
        mended.chair = ChairState::Mended;
        CHECK(!placementFor(mended, ItemKind::Quilt, aimAt(TargetKind::Table, -1, chair.point), yaw, p));
        CHECK(placementFor(mended, ItemKind::Quilt, chair, yaw, p));
        CHECK(p.pos.x == 1.65f && p.pos.y == 0.9f && p.pos.z == -0.9f);
        CHECK(p.yaw == 0);
    }

    // ---- the window: once every board is off, aimed at it or the wall just round it
    {
        const float midZ = (L::kWindowZ0 + L::kWindowZ1) / 2;
        const Aim board = aimAt(TargetKind::WindowBoard, 2, {L::kInnerX0, 1.7f, midZ});
        Pose p = kUntouched;
        CHECK(!placementFor(w, ItemKind::Curtains, board, yaw, p));
        World open = w;
        open.windowBoards = {0, 0, 1, 0};  // one board still on
        CHECK(!placementFor(open, ItemKind::Curtains, board, yaw, p));
        CHECK(untouched(p));
        open.windowBoards.fill(0);
        CHECK(placementFor(open, ItemKind::Curtains, board, yaw, p));
        CHECK(p.pos.x == L::kInnerX0 + 0.05f && p.pos.y == 2.3f && p.pos.z == midZ);
        CHECK(p.yaw == 0);

        // The left wall beside the window counts; further along it, or another wall, does not.
        p = kUntouched;
        CHECK(placementFor(open, ItemKind::Curtains, aimAt(TargetKind::Wall, 1, {L::kInnerX0, 1.5f, L::kWindowZ1 + 0.4f}), yaw, p));
        CHECK(p.pos.z == midZ);
        p = kUntouched;
        CHECK(!placementFor(open, ItemKind::Curtains, aimAt(TargetKind::Wall, 1, {L::kInnerX0, 1.5f, L::kWindowZ1 + 0.8f}), yaw, p));
        CHECK(!placementFor(open, ItemKind::Curtains, aimAt(TargetKind::Wall, 1, {L::kInnerX0, L::kWindowY0 - 0.5f, midZ}), yaw, p));
        CHECK(!placementFor(open, ItemKind::Curtains, aimAt(TargetKind::Wall, 2, {L::kInnerX1, 1.5f, midZ}), yaw, p));
        CHECK(untouched(p));
    }

    TEST_MAIN_END();
}
