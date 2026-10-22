// Ada's arrival: she walks up from her wagon once the hearth is lit, looks
// round the room, says what she thinks (it depends on her brief), and sits
// down by the fire.
#pragma once

#include <string>
#include <vector>

#include "sim/world.h"

namespace kindling {

constexpr float kSettlerSpeed = 1.4f;        // m/s, an unhurried walk
constexpr float kLineSeconds = 5.0f;         // each line stays up this long

// Ada's lines on arriving, in order, for the room as it is.
std::vector<std::string> settlerLines(const World& w);

// The line she is saying now, or empty.
std::string settlerSpeech(const World& w);

// Advance her one tick (nothing happens while she is away and the step is not Welcome).
struct TickEvents;
void updateSettler(World& w, TickEvents* ev);

}  // namespace kindling
