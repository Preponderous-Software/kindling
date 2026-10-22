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

    // Per-frame motion that is not part of the World: the flames flicker.
    // `seconds` is real time, so the flicker never touches the simulation.
    void animate(const World& w, double seconds);

    // The point lights for this moment: the fire, its glow, the candle, the
    // lantern in the player's hand. Flickers with `seconds` like the flames.
    void lights(const World& w, double seconds, std::vector<StrataPointLight>& out) const;

    // Re-uploads the grime texels a tick changed.
    void updateGrime(const World& w, const std::array<TexelRect, kSurfaceCount>& changed);

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

    uint32_t upload(const MeshData& m, uint32_t texture = 0);
    void uploadGrime(const World& w, Surface s, const TexelRect& r);
    void drop(Held& h);

    strata::Engine& engine_;
    std::array<Held, static_cast<size_t>(Part::Count)> parts_{};
    std::vector<Held> debris_;  // one per World::debris entry
    std::vector<Held> items_;   // one per World::items entry
    uint32_t flames_ = 0, candleFlame_ = 0;  // unlit meshes, not pickable
    std::unordered_map<uint32_t, const Held*> byId_;
    std::array<uint32_t, kSurfaceCount> grimeTextures_{};
    std::vector<uint8_t> scratch_;
};

}  // namespace kindling::bridge
