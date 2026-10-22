#include "sim/brief.h"

#include "sim/layout.h"

namespace kindling {

namespace L = kindling::layout;

namespace {

const Item* placed(const World& w, ItemKind k) {
    for (const Item& it : w.items)
        if (it.kind == k && it.place == ItemPlace::Placed) return &it;
    return nullptr;
}

}  // namespace

bool Brief::met() const { return metCount() == kBriefPoints; }

int Brief::metCount() const {
    int n = 0;
    for (const BriefPoint& p : points) n += p.met ? 1 : 0;
    return n;
}

Brief evaluateBrief(const World& w) {
    Brief b;
    bool windowOpen = true;
    for (int32_t board : w.windowBoards) windowOpen = windowOpen && board == 0;

    b.points[0] = {"A big table, in the window's light", w.table == TableState::Upright && windowOpen};
    b.points[1] = {"No dark corners: a candle, and curtains",
                   placed(w, ItemKind::Candle) && placed(w, ItemKind::Curtains)};
    b.points[2] = {"Somewhere to sit and sew, with a quilt to hand",
                   w.chair == ChairState::Mended && placed(w, ItemKind::Quilt)};
    const Item* rug = placed(w, ItemKind::Rug);
    const float hx = (L::kFireboxX0 + L::kFireboxX1) / 2, hz = L::kHearthstoneZ1;
    const bool rugAtHearth = rug && (rug->pos.x - hx) * (rug->pos.x - hx) + (rug->pos.z - hz) * (rug->pos.z - hz) < 2.0f * 2.0f;
    b.points[3] = {"A rug before the hearth", rugAtHearth};
    b.points[4] = {"Something of home on the walls", placed(w, ItemKind::Sampler) != nullptr};
    return b;
}

}  // namespace kindling
