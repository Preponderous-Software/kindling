#include "sim/items.h"

#include "sim/layout.h"
#include "sim/world.h"

namespace kindling {

namespace L = kindling::layout;

const char* itemName(ItemKind k) {
    switch (k) {
        case ItemKind::Plank: return "New plank";
        case ItemKind::Shingles: return "Bundle of shingles";
        case ItemKind::Rug: return "Braided rug";
        case ItemKind::Quilt: return "Patchwork quilt";
        case ItemKind::Curtains: return "Curtains";
        case ItemKind::Sampler: return "Framed sampler";
        case ItemKind::Crock: return "Stoneware crock";
        case ItemKind::Jar: return "Blue jar";
        case ItemKind::Clock: return "Mantel clock";
        case ItemKind::Candle: return "Candle";
        case ItemKind::Basket: return "Kindling basket";
        case ItemKind::Count: break;
    }
    return "";
}

bool isMaterial(ItemKind k) { return k == ItemKind::Plank || k == ItemKind::Shingles; }

Anchor anchorOf(ItemKind k) {
    switch (k) {
        case ItemKind::Rug:
        case ItemKind::Basket: return Anchor::Floor;
        case ItemKind::Crock:
        case ItemKind::Jar:
        case ItemKind::Clock:
        case ItemKind::Candle: return Anchor::Mantel;
        case ItemKind::Sampler: return Anchor::Wall;
        case ItemKind::Quilt: return Anchor::Chair;
        case ItemKind::Curtains: return Anchor::Window;
        default: return Anchor::None;
    }
}

const char* anchorHint(Anchor a) {
    switch (a) {
        case Anchor::Floor: return "That goes on the floor.";
        case Anchor::Mantel: return "That goes on the mantel shelf.";
        case Anchor::Wall: return "Hang that on a clean wall.";
        case Anchor::Chair: return "That goes over the back of the rocking chair, once it's mended.";
        case Anchor::Window: return "Hang those at the window, once the boards are off.";
        case Anchor::None: break;
    }
    return "";
}

std::vector<Item> newItems() {
    std::vector<Item> items;
    int32_t id = 1;
    auto add = [&](ItemKind k, float x, float z, float yaw = 0) {
        Item it;
        it.id = id++;
        it.kind = k;
        it.pos = {x, 0.86f, z};
        it.yaw = yaw;
        items.push_back(it);
    };
    // Lumber along the left of the bed, shingles beside it, furnishings at the back.
    for (int k = 0; k < 3; ++k) add(ItemKind::Plank, 1.65f + 0.12f * k, 8.2f, 1.5708f);
    for (int k = 0; k < 4; ++k) add(ItemKind::Shingles, 2.3f + 0.3f * (k % 2), 7.45f + 0.35f * (k / 2));
    add(ItemKind::Rug, 3.3f, 7.5f);
    add(ItemKind::Quilt, 3.25f, 8.0f);
    add(ItemKind::Curtains, 2.9f, 8.45f);
    add(ItemKind::Sampler, 3.35f, 8.55f);
    add(ItemKind::Crock, 2.3f, 8.3f);
    add(ItemKind::Jar, 2.55f, 8.35f);
    add(ItemKind::Clock, 2.4f, 8.75f);
    add(ItemKind::Candle, 2.75f, 8.8f);
    add(ItemKind::Basket, 3.1f, 9.0f);
    return items;
}

bool placementFor(const World& w, ItemKind kind, const Aim& aim, float yaw, Pose& out) {
    if (aim.kind == TargetKind::None || aim.distance > L::kReach) return false;
    const Vec3 p = aim.point;
    switch (anchorOf(kind)) {
        case Anchor::Floor: {
            const bool onFloor = aim.kind == TargetKind::Floor ||
                                 (aim.kind == TargetKind::RottenPlank && aim.index >= 0 &&
                                  w.planks[static_cast<size_t>(aim.index)] == PlankState::New);
            if (!onFloor || !L::kInterior.contains(p.x, p.z)) return false;
            out = {{p.x, L::kFloorY, p.z}, yaw};
            return true;
        }
        case Anchor::Mantel:
            // Aiming at the shelf itself, or at the stones just above it.
            if (aim.kind != TargetKind::HearthFace) return false;
            if (p.x < L::kMantelX0 + 0.1f || p.x > L::kMantelX1 - 0.1f) return false;
            if (p.y < L::kMantelY - 0.15f || p.y > L::kMantelY + 0.5f) return false;
            out = {{p.x, L::kMantelY, (L::kMantelZ0 + L::kMantelZ1) / 2}, 0};
            return true;
        case Anchor::Wall: {
            if (aim.kind != TargetKind::Wall && aim.kind != TargetKind::HearthFace) return false;
            if (p.y < 1.2f || p.y > 2.5f) return false;
            // Face into the room from whichever wall it hangs on.
            float facing = 0;
            if (aim.kind == TargetKind::Wall) {
                switch (aim.index) {
                    case 1: facing = 1.5708f; break;    // left wall, faces +x
                    case 2: facing = -1.5708f; break;   // right wall, faces -x
                    case 3: facing = 3.14159f; break;   // front wall, faces -z
                    default: facing = 0; break;         // back wall, faces +z
                }
            }
            out = {p, facing};
            return true;
        }
        case Anchor::Chair:
            if (aim.kind != TargetKind::Chair || w.chair != ChairState::Mended) return false;
            out = {{1.65f, 0.9f, -0.9f}, 0};
            return true;
        case Anchor::Window: {
            bool open = true;
            for (int32_t b : w.windowBoards) open = open && b == 0;
            const bool atWindow = aim.kind == TargetKind::WindowBoard ||
                                  (aim.kind == TargetKind::Wall && aim.index == 1 && p.z > L::kWindowZ0 - 0.6f &&
                                   p.z < L::kWindowZ1 + 0.6f && p.y > L::kWindowY0 - 0.3f);
            if (!open || !atWindow) return false;
            out = {{L::kInnerX0 + 0.05f, 2.3f, (L::kWindowZ0 + L::kWindowZ1) / 2}, 0};
            return true;
        }
        case Anchor::None: break;
    }
    return false;
}

}  // namespace kindling
