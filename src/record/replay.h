// The restoration as a usd-replay/1 replay (usd-recorder's contract), written
// from the reveal's keyframes with usd-recorder's dependency-free writer. Any
// player of the contract can show it: usd-web in a browser, usdview, Blender.
#pragma once

#include <string>

#include "sim/world.h"

namespace kindling {

// Writes the world's restoration history to `path` (.usda). False, with a
// reason in *error, if there is no history or the file cannot be written.
bool writeReplay(const World& w, const std::string& path, std::string* error = nullptr);

}  // namespace kindling
