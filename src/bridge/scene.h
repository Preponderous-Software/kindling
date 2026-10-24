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

    // The point lights for this moment: daylight coming in (through the door,
    // the roof hole and the window, scaled by `daylight` 0..1 and by how open
    // each is), the fire, its glow, the candle, the lantern in the player's
    // hand. Flickers with `seconds` like the flames.
    void lights(const World& w, double seconds, float daylight, std::vector<StrataPointLight>& out) const;

    // The tool in hand, in the corner of the view, swinging while it works.
    // `working` is whether the work under the crosshair is going on.
    void animateTool(const World& w, double seconds, bool working);

    // A puff of dust (or snow, or ash) where a tool is working; drawn by
    // drawDust between begin and end frame. Real time, like the flicker.
    void puff(Vec3 at, Rgb color, double seconds);
    // The dust, and snow sifting down through each open section of the roof,
    // so the holes show from across the room.
    void drawDust(const World& w, double seconds);

    // Show the player's own figure (in the reveal, seen from outside).
    void showPlayer(bool on) { showPlayer_ = on; }

    // Re-uploads every grime texel (after jumping to a different world, as
    // the reveal does).
    void reloadGrime(const World& w);

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
    void keep(Held& h, const MeshData& m);

    strata::Engine& engine_;
    std::array<Held, static_cast<size_t>(Part::Count)> parts_{};
    std::vector<Held> debris_;  // one per World::debris entry
    std::vector<Held> items_;   // one per World::items entry
    uint32_t flames_ = 0, candleFlame_ = 0;  // unlit meshes, not pickable
    uint32_t settler_ = 0, homesteader_ = 0;  // figures, not pickable
    Held horseBody_, horseHead_, horseTail_;  // the mare, pickable
    int horseCoat_ = -1;                       // the dirt level her meshes were built with (tenths)
    uint32_t tool_ = 0;                        // the tool in hand, not pickable
    Tool toolBuilt_ = Tool::None;
    struct Mote {
        Vec3 pos, vel;
        Rgb color;
        double born = 0;
    };
    std::vector<Mote> dust_;
    double lastPuff_ = 0;
    uint32_t rng_ = 0x2545f491u;
    bool showPlayer_ = false;
    std::unordered_map<uint32_t, const Held*> byId_;
    std::array<uint32_t, kSurfaceCount> grimeTextures_{};
    std::vector<uint8_t> scratch_;
};

}  // namespace kindling::bridge
