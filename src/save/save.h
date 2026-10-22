// Saving and continuing.
//
// A save is one file in the slot directory:
//
//   "KINDLING"  8 bytes, the magic
//   version     u32 little-endian (kSaveVersion)
//   length      u64 little-endian, bytes of payload
//   payload     the World, field by field, little-endian
//   sha256      32 bytes, over the payload
//
// The checksum is verified before a byte of the payload is read, and every
// read is bounds-checked, so a damaged or truncated save is refused, never
// half-loaded. Writing goes to a temporary file first and is renamed into
// place; the save it replaces is kept as the previous save, which is loaded
// instead when the current one is damaged.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sim/world.h"

namespace kindling {

constexpr uint32_t kSaveVersion = 1;

// The World as save bytes, and back. decode returns false (with a reason)
// on anything it does not accept.
std::vector<uint8_t> encodeSave(const World& w);
bool decodeSave(const std::vector<uint8_t>& bytes, World& out, std::string* error);

struct LoadResult {
    bool ok = false;
    bool fromPrevious = false;  // the current save was damaged; the previous one was loaded
    std::string message;        // what happened, for the player
};

// The slot's files.
std::string savePath(const std::string& slot);
std::string previousSavePath(const std::string& slot);

// Writes the world to the slot, keeping the save it replaces as the previous one.
bool writeSave(const std::string& slot, const World& w, std::string* error);
// Loads the slot's save, or its previous save if the current one is damaged.
LoadResult loadSave(const std::string& slot, World& out);

// Where saves go when no --slot is given: %APPDATA%\Kindling\default on
// Windows, $XDG_DATA_HOME/kindling/default (or ~/.local/share/...) elsewhere.
std::string defaultSlot();

}  // namespace kindling
