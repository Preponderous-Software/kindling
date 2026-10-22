// Ada Lark's brief for her home: what she asked for, checked against the room.
//
// Ada is a seamstress, particular about light. Her words: "A south-facing
// sewing room, a big table, and no dark corners." The MVP's one room is read
// against that brief point by point.
#pragma once

#include <array>
#include <cstdint>

#include "sim/world.h"

namespace kindling {

struct BriefPoint {
    const char* wish;  // in Ada's terms
    bool met = false;
};

constexpr int kBriefPoints = 5;

struct Brief {
    std::array<BriefPoint, kBriefPoints> points{};
    bool met() const;
    int metCount() const;
};

Brief evaluateBrief(const World& w);

}  // namespace kindling
