// The cabin and yard as meshes, built from the World with no engine involved.
//
// The scene is split into parts so that a change (a plank laid, a board pried
// off) rebuilds one small mesh, not the room. Each part says which World fields
// it depends on through a version key; the renderer rebuilds a part only when
// its key changes.
#pragma once

#include <cstdint>
#include <vector>

#include "bridge/geometry.h"
#include "sim/world.h"

namespace kindling::bridge {

enum class Part : int32_t {
    Walls = 0,
    Floor,
    Ceiling,
    Roof,
    Chimney,
    Mantel,
    Window,
    Boards,
    Chair,
    Table,
    Snow,
    Yard,
    Ground,
    Count
};

// The mesh for one part in the world's current state.
MeshData buildPart(Part part, const World& w);

// A key that changes whenever the part's mesh would.
uint64_t partVersion(Part part, const World& w);

// One piece of debris, built around the origin; place it with debrisTransform.
MeshData buildDebris(const Debris& d, int32_t index);
// Column-major model matrix for a piece of debris.
void debrisTransform(const Debris& d, float out[16]);

}  // namespace kindling::bridge
