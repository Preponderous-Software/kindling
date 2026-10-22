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
    WallBack = 0,
    WallLeft,
    WallRight,
    WallFront,
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
    FireboxContents,  // ash, kindling, logs laid on the andirons
    YardWood,         // rounds on the pile and the block, split logs and kindling by it
    Woodpile,         // split logs stacked inside, left of the hearth
    Count
};

// The mesh for one part in the world's current state. Parts with a grime
// surface (partSurface) come with uvs that map onto that surface's texels.
MeshData buildPart(Part part, const World& w);

// The grime surface a part is textured with, or false for untextured parts.
bool partSurface(Part part, Surface& out);

// A key that changes whenever the part's mesh would.
uint64_t partVersion(Part part, const World& w);

// The flames, built around the firebox's centre on the andirons, unlit; the
// renderer scales them with the fire each frame.
MeshData buildFlames();
// A candle flame, built around its base.
MeshData buildCandleFlame();

// One material or furnishing, built around its origin; place it with itemTransform.
MeshData buildItem(const Item& item, int32_t index);
void itemTransform(const Item& item, float out[16]);
// Half the footprint of a furnishing on its anchor (x, z half-extents), for
// the placement outline.
void itemFootprint(ItemKind kind, float& halfX, float& halfZ);

// One piece of debris, built around the origin; place it with debrisTransform.
MeshData buildDebris(const Debris& d, int32_t index);
// Column-major model matrix for a piece of debris.
void debrisTransform(const Debris& d, float out[16]);

}  // namespace kindling::bridge
