// Keeps Strata's meshes in step with the World, and turns a picked mesh
// triangle back into the game target it belongs to.
#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "bridge/geometry.h"
#include "bridge/room.h"
#include "sim/world.h"
#include "strata/strata.hpp"

namespace kindling::bridge {

class Scene {
public:
    explicit Scene(strata::Engine& engine) : engine_(engine) {}
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Rebuilds whatever changed since the last call (everything on the first).
    void sync(const World& w);

    // What the crosshair ray from `eye` along `dir` (unit) meets first, out to
    // maxDistance. Uses Strata's mesh picking plus boxes for the roof hole.
    Aim pick(const World& w, Vec3 eye, Vec3 dir, float maxDistance) const;

    // Total meshes held (for the HUD's debug line).
    size_t meshCount() const;

private:
    struct Held {
        uint32_t id = 0;               // Strata mesh id, 0 when the part is empty
        uint64_t version = ~0ull;      // partVersion when built
        std::vector<TriTag> tags;
    };

    uint32_t upload(const MeshData& m);
    void drop(Held& h);

    strata::Engine& engine_;
    std::array<Held, static_cast<size_t>(Part::Count)> parts_{};
    std::vector<Held> debris_;  // one per World::debris entry
    std::unordered_map<uint32_t, const Held*> byId_;
};

}  // namespace kindling::bridge
