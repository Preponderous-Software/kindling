// M5 Welcome and reveal, the welcome: Ada stays at her wagon until the hearth is lit, walks in, looks
// round while she says her first line, crosses to the chair and sits; what she
// says depends on the brief, as she found it when she arrived.
#include <cmath>
#include <string>

#include "check.h"
#include "sim/brief.h"
#include "sim/layout.h"
#include "sim/rules.h"
#include "sim/settler.h"

using namespace kindling;
namespace L = kindling::layout;

namespace {

int32_t firstItem(const World& w, ItemKind k) {
    for (size_t i = 0; i < w.items.size(); ++i)
        if (w.items[i].kind == k && w.items[i].place == ItemPlace::Wagon) return static_cast<int32_t>(i);
    return -1;
}

void place(World& w, ItemKind k, Vec3 p) {
    Item& it = w.items[static_cast<size_t>(firstItem(w, k))];
    it.place = ItemPlace::Placed;
    it.pos = p;
}

// Every point of the brief met, as in the repair-and-furnish test.
void meetBrief(World& w) {
    w.windowBoards.fill(0);
    w.table = TableState::Upright;
    w.chair = ChairState::Mended;
    place(w, ItemKind::Candle, {0.5f, L::kMantelY, -2.3f});
    place(w, ItemKind::Curtains, {L::kInnerX0, 2.3f, -0.3f});
    place(w, ItemKind::Quilt, {1.65f, 0.9f, -0.9f});
    place(w, ItemKind::Sampler, {0, 1.8f, L::kInnerZ0});
    place(w, ItemKind::Rug, {-0.8f, L::kFloorY, -1.0f});
}

// Calls updateSettler until her state changes; returns the number of calls, or -1.
int untilStateChanges(World& w, TickEvents* ev = nullptr, int limit = 100000) {
    const SettlerState from = w.settler.state;
    for (int n = 1; n <= limit; ++n) {
        updateSettler(w, ev);
        if (w.settler.state != from) return n;
    }
    return -1;
}

}  // namespace

int main() {
    // ---- she waits at the wagon until the hearth is lit
    {
        World w = newWorld(3);
        const Settler start = w.settler;
        for (int i = 0; i < 300; ++i) updateSettler(w, nullptr);
        CHECK(w.settler.state == SettlerState::Away);
        CHECK(w.settler.x == start.x && w.settler.z == start.z);
        CHECK(w.settler.line == -1);
        CHECK(settlerSpeech(w).empty());

        w.kindled = true;
        updateSettler(w, nullptr);
        CHECK(w.settler.state == SettlerState::Arriving);
        CHECK(w.settler.waypoint == 1);
        CHECK(w.settler.x == start.x && w.settler.z == start.z);  // she sets off next tick
    }

    // ---- the walk in takes as long as 7.2 m at her pace, and she says she's here
    {
        World w = newWorld(3);
        w.kindled = true;
        updateSettler(w, nullptr);
        TickEvents ev;
        updateSettler(w, &ev);
        CHECK(w.settler.z < 10.5f);
        CHECK(std::fabs(w.settler.yaw) < 1e-4f);  // walking toward -z
        const int ticks = 1 + untilStateChanges(w, &ev);
        const int expected = static_cast<int>(7.2f / (kSettlerSpeed * kTickSeconds));
        CHECK(ticks >= expected && ticks <= expected + 3);  // each leg ends on a part step
        CHECK(w.settler.state == SettlerState::LookingAround);
        CHECK(w.settler.x == 2.5f && w.settler.z == 3.3f);
        CHECK(w.settler.line == 0);
        CHECK(ev.messages.size() == 1 && ev.messages[0] == "Ada Lark is here.");
    }

    // ---- what she finds is fixed when she arrives
    {
        World w = newWorld(3);
        w.kindled = true;
        untilStateChanges(w);
        untilStateChanges(w);
        CHECK(w.settler.state == SettlerState::LookingAround);
        CHECK(!w.settler.briefMet);
        meetBrief(w);
        CHECK(evaluateBrief(w).met());
        updateSettler(w, nullptr);
        CHECK(!w.settler.briefMet);
        // So she still speaks as one who found it wanting, though nothing is left to ask for.
        CHECK(settlerLines(w).size() == 2);
        CHECK(settlerLines(w).front() == "Oh, it's warm. That's a fine start.");

        World met = newWorld(3);
        meetBrief(met);
        met.kindled = true;
        untilStateChanges(met);
        untilStateChanges(met);
        CHECK(met.settler.briefMet);
    }

    // ---- her lines: the brief met, or the first two things still wanting
    {
        World w = newWorld(3);
        meetBrief(w);
        w.settler.briefMet = true;
        const std::vector<std::string> met = settlerLines(w);
        CHECK(met.size() == 4);
        CHECK(met.front() == "Oh... it's warm.");
        CHECK(met.back() == "This is home. Thank you.");

        World bare = newWorld(3);
        const Brief b = evaluateBrief(bare);
        CHECK(b.metCount() == 0);
        const std::vector<std::string> wanting = settlerLines(bare);
        CHECK(wanting.size() == 4);  // she names two wishes, not all five
        CHECK(wanting[1] == std::string("I'll still want: ") + b.points[0].wish + ".");
        CHECK(wanting[2] == std::string("I'll still want: ") + b.points[1].wish + ".");
        CHECK(wanting.back() == "But it's warm, and it's mine. Thank you.");

        // One point short: the rug left by the door.
        World nearly = newWorld(3);
        meetBrief(nearly);
        nearly.items[static_cast<size_t>(firstItem(newWorld(3), ItemKind::Rug))].pos = {3.0f, L::kFloorY, 3.0f};
        const Brief nb = evaluateBrief(nearly);
        CHECK(nb.metCount() == kBriefPoints - 1 && !nb.points[3].met);
        const std::vector<std::string> one = settlerLines(nearly);
        CHECK(one.size() == 3);
        CHECK(one[1] == std::string("I'll still want: ") + nb.points[3].wish + ".");
    }

    // ---- settlerSpeech is the current line, and nothing outside the list
    {
        World w = newWorld(3);
        const std::vector<std::string> lines = settlerLines(w);
        w.settler.line = 0;
        CHECK(settlerSpeech(w) == lines[0]);
        w.settler.line = static_cast<int32_t>(lines.size()) - 1;
        CHECK(settlerSpeech(w) == lines.back());
        w.settler.line = static_cast<int32_t>(lines.size());
        CHECK(settlerSpeech(w).empty());
        w.settler.line = -1;
        CHECK(settlerSpeech(w).empty());
    }

    // ---- she looks round for one line, then sits facing the fire until every line is said
    {
        World w = newWorld(3);
        meetBrief(w);
        w.kindled = true;
        untilStateChanges(w);
        untilStateChanges(w);
        const float yaw0 = w.settler.yaw;
        const int looking = untilStateChanges(w);
        CHECK(looking == static_cast<int>(kLineSeconds * kTickRate) ||
              looking == static_cast<int>(kLineSeconds * kTickRate) + 1);
        CHECK(w.settler.yaw > yaw0);  // turning slowly, taking it in
        CHECK(w.settler.state == SettlerState::ToChair);
        CHECK(w.settler.line == 1);
        CHECK(!w.settler.done);

        untilStateChanges(w);
        CHECK(w.settler.state == SettlerState::Seated);
        CHECK(w.settler.x == 1.65f && w.settler.z == -0.8f);
        CHECK(w.settler.yaw == 0);
        CHECK(!w.settler.done);  // still talking

        int n = 0;
        while (!w.settler.done && n < 100000) {
            updateSettler(w, nullptr);
            ++n;
        }
        CHECK(w.settler.done);
        CHECK(w.settler.line == -1);
        CHECK(settlerSpeech(w).empty());
        CHECK(w.settler.state == SettlerState::Seated);
    }

    TEST_MAIN_END();
}
