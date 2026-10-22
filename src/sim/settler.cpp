#include "sim/settler.h"

#include <cmath>
#include <iterator>

#include "sim/brief.h"
#include "sim/rules.h"

namespace kindling {

namespace {

struct Waypoint {
    float x, z;
};
// From her wagon up the path, through the door, to a spot just inside.
constexpr Waypoint kWalkIn[] = {{2.5f, 10.5f}, {2.5f, 5.0f}, {2.5f, 3.3f}};
// From there to the rocking chair, which faces the hearth.
constexpr Waypoint kToChair[] = {{2.0f, 0.4f}, {1.65f, -0.8f}};

// Walks toward (x, z); true on arrival.
bool walk(Settler& s, Waypoint to) {
    const float dx = to.x - s.x, dz = to.z - s.z;
    const float d = std::sqrt(dx * dx + dz * dz);
    const float step = kSettlerSpeed * kTickSeconds;
    if (d <= step) {
        s.x = to.x;
        s.z = to.z;
        return true;
    }
    s.x += dx / d * step;
    s.z += dz / d * step;
    // Yaw 0 faces -z; turning right is positive.
    s.yaw = detAtan2(dx, -dz);
    return false;
}

}  // namespace

std::vector<std::string> settlerLines(const World& w) {
    std::vector<std::string> lines;
    if (w.settler.briefMet) {
        lines.push_back("Oh... it's warm.");
        lines.push_back("You put the table where the light falls. And a candle. No dark corners.");
        lines.push_back("A quilt on the chair, by the fire. You knew I'd want to sew here.");
        lines.push_back("This is home. Thank you.");
        return lines;
    }
    lines.push_back("Oh, it's warm. That's a fine start.");
    const Brief b = evaluateBrief(w);
    int said = 0;
    for (const BriefPoint& p : b.points)
        if (!p.met && said < 2) {
            lines.push_back(std::string("I'll still want: ") + p.wish + ".");
            ++said;
        }
    lines.push_back("But it's warm, and it's mine. Thank you.");
    return lines;
}

std::string settlerSpeech(const World& w) {
    if (w.settler.line < 0) return {};
    const std::vector<std::string> lines = settlerLines(w);
    return static_cast<size_t>(w.settler.line) < lines.size() ? lines[static_cast<size_t>(w.settler.line)] : std::string();
}

void updateSettler(World& w, TickEvents* ev) {
    Settler& s = w.settler;
    switch (s.state) {
        case SettlerState::Away:
            if (!w.kindled) return;
            s.state = SettlerState::Arriving;
            s.waypoint = 1;
            if (ev) ev->messages.push_back("Ada Lark is here.");
            return;
        case SettlerState::Arriving:
            if (walk(s, kWalkIn[s.waypoint]) && ++s.waypoint >= static_cast<int32_t>(std::size(kWalkIn))) {
                s.state = SettlerState::LookingAround;
                s.briefMet = evaluateBrief(w).met();
                s.line = 0;
                s.lineSeconds = 0;
            }
            return;
        case SettlerState::LookingAround:
            // She turns slowly, taking it in, through her first line.
            s.yaw += 0.5f * kTickSeconds;
            s.lineSeconds += kTickSeconds;
            if (s.lineSeconds >= kLineSeconds) {
                s.state = SettlerState::ToChair;
                s.waypoint = 0;
                s.line = 1;
                s.lineSeconds = 0;
            }
            return;
        case SettlerState::ToChair:
        case SettlerState::Seated: {
            if (s.state == SettlerState::ToChair && walk(s, kToChair[s.waypoint]) &&
                ++s.waypoint >= static_cast<int32_t>(std::size(kToChair))) {
                s.state = SettlerState::Seated;
                s.yaw = 0;  // facing the fire
            }
            if (s.line < 0) return;
            s.lineSeconds += kTickSeconds;
            if (s.lineSeconds >= kLineSeconds) {
                s.lineSeconds = 0;
                if (++s.line >= static_cast<int32_t>(settlerLines(w).size())) {
                    s.line = -1;
                    if (s.state == SettlerState::Seated) s.done = true;
                }
            }
            if (s.line < 0 && s.state == SettlerState::Seated) s.done = true;
            return;
        }
    }
}

}  // namespace kindling
