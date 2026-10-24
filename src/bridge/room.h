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

// The grime texture's colour over a swept patch of floor. The floor's own
// colours are divided by it, so swept boards show as built while dust and
// leaves show paler and greyer than the wood under them.
constexpr Rgb kFloorSwept{0.67f, 0.53f, 0.36f};

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

// People, built standing around their feet: Ada (a plum dress, a shawl, her
// hair up), and the player as seen in the reveal (work clothes and a hat).
MeshData buildSettler();
MeshData buildHomesteader();
// Model matrix for a figure at (x, z) facing yaw, lowered when seated.
void figureTransform(float x, float z, float yaw, bool seated, float out[16]);

// The mare, as three rigid parts in her own frame (facing -z, hooves at y 0).
// The head and tail are built around their pivots so they can turn.
constexpr Vec3 kHorseHeadPivot{0, 1.45f, -0.7f}, kHorseTailPivot{0, 1.35f, 0.85f};
// The rocking chair's seat, where Ada is drawn once she sits (the rules leave
// her standing at the end of her walk, behind the chair's back).
constexpr float kChairSeatX = 1.65f, kChairSeatZ = -1.1f;
MeshData buildHorseBody(float dirt);
MeshData buildHorseHead(float dirt);
MeshData buildHorseTail();
// Model matrix: the mare's frame at her stall position, times a part's pose
// (rotation about y by yaw, then x by pitch, about `pivot`).
void horsePartTransform(Vec3 pivot, float yaw, float pitch, float roll, float out[16]);

// One material or furnishing, built around its origin; place it with itemTransform.
MeshData buildItem(const Item& item, int32_t index);
void itemTransform(const Item& item, float out[16]);
// Half the footprint of a furnishing on its anchor (x, z half-extents), for
// the placement outline.
void itemFootprint(ItemKind kind, float& halfX, float& halfZ);

// The tool in the player's hand, built around the hand in view space (x right,
// y up, +z toward the eye); empty for no tool.
MeshData buildTool(Tool tool);

// One piece of debris as a capsule along its length (world space), for a
// forgiving pick: a flat board is a thin target from eye height.
void debrisCapsule(const Debris& d, Vec3& a, Vec3& b, float& radius);

// One piece of debris, built around the origin; place it with debrisTransform.
MeshData buildDebris(const Debris& d, int32_t index);
// Column-major model matrix for a piece of debris.
void debrisTransform(const Debris& d, float out[16]);

}  // namespace kindling::bridge
