#include "bridge/scene.h"

#include <cmath>

#include "sim/layout.h"

namespace kindling::bridge {

namespace L = kindling::layout;

namespace {

// What grime looks like over a surface, as a multiplier on its own colour:
// dust and leaves on the floor, dust on the walls, soot on the chimney.
void grimeTexel(Surface s, uint8_t g, uint8_t* out) {
    const float t = static_cast<float>(g) / 255.0f;
    float tint[3];
    switch (s) {
        case Surface::Floor:
            tint[0] = 0.40f; tint[1] = 0.36f; tint[2] = 0.31f;
            break;
        case Surface::HearthFace:
            tint[0] = 0.22f; tint[1] = 0.21f; tint[2] = 0.20f;
            break;
        default:
            tint[0] = 0.50f; tint[1] = 0.48f; tint[2] = 0.45f;
    }
    for (int c = 0; c < 3; ++c) out[c] = static_cast<uint8_t>(255.0f * (1.0f + (tint[c] - 1.0f) * t) + 0.5f);
    out[3] = 255;
}

}  // namespace

Scene::~Scene() {
    for (Held& h : parts_) drop(h);
    for (Held& h : debris_) drop(h);
    for (Held& h : items_) drop(h);
    if (flames_) engine_.removeMesh(flames_);
    if (settler_) engine_.removeMesh(settler_);
    drop(horseBody_);
    drop(horseHead_);
    drop(horseTail_);
    if (homesteader_) engine_.removeMesh(homesteader_);
    if (candleFlame_) engine_.removeMesh(candleFlame_);
    for (uint32_t t : grimeTextures_)
        if (t) engine_.destroyTexture(t);
}

uint32_t Scene::upload(const MeshData& m, uint32_t texture) {
    if (m.empty()) return 0;
    StrataMesh mesh{};
    mesh.positions = m.positions.data();
    mesh.colors = m.colors.data();
    mesh.indices = m.indices.data();
    mesh.vertex_count = static_cast<int32_t>(m.vertexCount());
    mesh.index_count = static_cast<int32_t>(m.indices.size());
    mesh.color[3] = 1;
    if (!texture || m.uvs.empty()) return engine_.addMesh(mesh);
    StrataTexturedMesh tm{};
    tm.mesh = mesh;
    tm.uvs = m.uvs.data();
    tm.texture = texture;
    return engine_.addTexturedMesh(tm);
}

void Scene::uploadGrime(const World& w, Surface s, const TexelRect& r) {
    const SurfaceDef& d = surfaceDef(s);
    scratch_.resize(static_cast<size_t>(r.w * r.h * 4));
    for (int j = 0; j < r.h; ++j)
        for (int i = 0; i < r.w; ++i)
            grimeTexel(s, w.grime.at(s, r.x + i, r.y + j), &scratch_[static_cast<size_t>((j * r.w + i) * 4)]);
    uint32_t& tex = grimeTextures_[static_cast<size_t>(s)];
    if (!tex) tex = engine_.createTextureRgba8(d.width, d.height, scratch_.data(), STRATA_TEXTURE_CLAMP_U | STRATA_TEXTURE_CLAMP_V);
    else engine_.updateTextureRgba8(tex, r.x, r.y, r.w, r.h, scratch_.data());
}

void Scene::reloadGrime(const World& w) {
    for (int s = 0; s < kSurfaceCount; ++s) {
        const SurfaceDef& d = surfaceDef(static_cast<Surface>(s));
        uploadGrime(w, static_cast<Surface>(s), {0, 0, d.width, d.height});
    }
}

void Scene::updateGrime(const World& w, const std::array<TexelRect, kSurfaceCount>& changed) {
    for (int s = 0; s < kSurfaceCount; ++s)
        if (changed[static_cast<size_t>(s)].w) uploadGrime(w, static_cast<Surface>(s), changed[static_cast<size_t>(s)]);
}

void Scene::keep(Held& h, const MeshData& m) {
    drop(h);
    h.id = upload(m);
    h.tags = m.tags;
    if (h.id) byId_[h.id] = &h;
}

void Scene::drop(Held& h) {
    if (h.id) {
        engine_.removeMesh(h.id);
        byId_.erase(h.id);
    }
    h.id = 0;
    h.tags.clear();
}

void Scene::sync(const World& w) {
    for (int s = 0; s < kSurfaceCount; ++s)
        if (!grimeTextures_[static_cast<size_t>(s)]) {
            const SurfaceDef& d = surfaceDef(static_cast<Surface>(s));
            uploadGrime(w, static_cast<Surface>(s), {0, 0, d.width, d.height});
        }
    for (int p = 0; p < static_cast<int>(Part::Count); ++p) {
        const Part part = static_cast<Part>(p);
        Held& h = parts_[p];
        const uint64_t v = partVersion(part, w);
        if (h.version == v) continue;
        drop(h);
        const MeshData m = buildPart(part, w);
        Surface surface;
        h.id = upload(m, partSurface(part, surface) ? grimeTextures_[static_cast<size_t>(surface)] : 0);
        h.tags = m.tags;
        h.version = v;
        if (h.id) byId_[h.id] = &h;
    }

    if (debris_.size() != w.debris.size()) {
        for (Held& h : debris_) drop(h);
        debris_.assign(w.debris.size(), Held{});
    }
    for (size_t i = 0; i < w.debris.size(); ++i) {
        const Debris& d = w.debris[i];
        Held& h = debris_[i];
        if (d.hauled) {
            drop(h);
            continue;
        }
        if (!h.id) {
            const MeshData m = buildDebris(d, static_cast<int32_t>(i));
            h.id = upload(m);
            h.tags = m.tags;
            byId_[h.id] = &h;
        }
        float model[16];
        debrisTransform(d, model);
        engine_.setMeshTransform(h.id, model);
    }

    if (items_.size() != w.items.size()) {
        for (Held& h : items_) drop(h);
        items_.assign(w.items.size(), Held{});
    }
    for (size_t i = 0; i < w.items.size(); ++i) {
        const Item& it = w.items[i];
        Held& h = items_[i];
        if (it.place == ItemPlace::Used) {
            drop(h);
            continue;
        }
        if (!h.id) {
            const MeshData m = buildItem(it, static_cast<int32_t>(i));
            h.id = upload(m);
            h.tags = m.tags;
            byId_[h.id] = &h;
        }
        float model[16];
        itemTransform(it, model);
        engine_.setMeshTransform(h.id, model);
    }
}

Aim Scene::pick(const World& w, Vec3 eye, Vec3 dir, float maxDistance) const {
    Aim aim;
    float best = maxDistance;

    const float origin[3] = {eye.x, eye.y, eye.z}, direction[3] = {dir.x, dir.y, dir.z};
    const StrataRayHit hit = engine_.raycastMeshes(origin, direction, maxDistance);
    if (hit.mesh_id) {
        const auto it = byId_.find(hit.mesh_id);
        if (it != byId_.end()) {
            for (const TriTag& t : it->second->tags)
                if (static_cast<uint32_t>(hit.triangle) >= t.first &&
                    static_cast<uint32_t>(hit.triangle) < t.first + t.count) {
                    aim.kind = t.kind;
                    aim.index = t.index;
                }
            aim.point = {hit.point[0], hit.point[1], hit.point[2]};
            aim.distance = hit.distance;
            best = hit.distance;
        }
    }

    // The roof hole has no mesh to hit: test the ray against the ceiling plane.
    if (dir.y > 1e-4f && w.roofPatched != kRoofAllPatched) {
        const float t = (L::kCeilingY - eye.y) / dir.y;
        const Vec3 p = eye + dir * t;
        if (t > 0 && t < best && p.x > L::kRoofHoleX0 && p.x < L::kRoofHoleX1 && p.z > L::kRoofHoleZ0 &&
            p.z < L::kRoofHoleZ1) {
            const int i = p.x < (L::kRoofHoleX0 + L::kRoofHoleX1) / 2 ? 0 : 1;
            const int j = p.z < (L::kRoofHoleZ0 + L::kRoofHoleZ1) / 2 ? 0 : 1;
            if (!((w.roofPatched >> (i + 2 * j)) & 1u)) {
                aim.kind = TargetKind::RoofHole;
                aim.index = i + 2 * j;
                aim.point = p;
                aim.distance = t;
            }
        }
    }
    return aim;
}

namespace {

uint32_t uploadUnlit(strata::Engine& engine, const MeshData& m) {
    StrataTexturedMesh tm{};
    tm.mesh.positions = m.positions.data();
    tm.mesh.colors = m.colors.data();
    tm.mesh.indices = m.indices.data();
    tm.mesh.vertex_count = static_cast<int32_t>(m.vertexCount());
    tm.mesh.index_count = static_cast<int32_t>(m.indices.size());
    tm.mesh.color[3] = 1;
    tm.flags = STRATA_MESH_UNLIT;
    return engine.addTexturedMesh(tm);
}

void placeAt(float out[16], Vec3 p, float sx, float sy, float sz) {
    const float m[16] = {sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, sz, 0, p.x, p.y, p.z, 1};
    for (int i = 0; i < 16; ++i) out[i] = m[i];
}

// Flicker in 0.75..1.0, a sum of out-of-step waves so it never looks periodic.
float flicker(double t, float phase) {
    const double v = 0.5 * std::sin(t * 13.1 + phase) + 0.3 * std::sin(t * 7.3 + 2.0 * phase) + 0.2 * std::sin(t * 23.7);
    return static_cast<float>(0.875 + 0.125 * v);
}

const Item* placedCandle(const World& w) {
    for (const Item& it : w.items)
        if (it.kind == ItemKind::Candle && it.place == ItemPlace::Placed) return &it;
    return nullptr;
}

constexpr float kHidden = -100.0f;  // below the ground: an unlit mesh "switched off"
const Vec3 kFireBase{(L::kFireboxX0 + L::kFireboxX1) / 2, 0.22f, -2.6f};

}  // namespace

void Scene::animate(const World& w, double seconds) {
    if (!flames_) flames_ = uploadUnlit(engine_, buildFlames());
    if (!candleFlame_) candleFlame_ = uploadUnlit(engine_, buildCandleFlame());
    float model[16];
    const float fire = w.hearth.lit ? w.hearth.fire : 0.0f;
    if (fire > 0.01f) {
        const float f = flicker(seconds, 0.0f);
        placeAt(model, kFireBase, 0.5f + 0.5f * fire, fire * f, 0.7f + 0.3f * fire);
    } else {
        placeAt(model, {0, kHidden, 0}, 1, 1, 1);
    }
    engine_.setMeshTransform(flames_, model);

    const Item* candle = placedCandle(w);
    if (candle && w.hearth.candleLit) placeAt(model, candle->pos + Vec3{0, 0.23f, 0}, 1, flicker(seconds, 1.7f), 1);
    else placeAt(model, {0, kHidden, 0}, 1, 1, 1);
    engine_.setMeshTransform(candleFlame_, model);

    if (!settler_) settler_ = upload(buildSettler());
    if (!homesteader_) homesteader_ = upload(buildHomesteader());
    const Settler& s = w.settler;
    if (s.state == SettlerState::Away) placeAt(model, {0, kHidden, 0}, 1, 1, 1);
    else figureTransform(s.x, s.z, s.yaw, s.state == SettlerState::Seated, model);
    engine_.setMeshTransform(settler_, model);
    if (showPlayer_) figureTransform(w.player.x, w.player.z, w.player.yaw, false, model);
    else placeAt(model, {0, kHidden, 0}, 1, 1, 1);
    engine_.setMeshTransform(homesteader_, model);

    // The mare: her coat follows how clean she is; her head turns to you when
    // you are near and drops to her hay while she eats; her tail swishes, more
    // when she is content; she leans into the brush.
    const Horse& h = w.horse;
    const int coatLevel = static_cast<int>(h.dirt * 10.0f + 0.5f);
    if (coatLevel != horseCoat_) {
        keep(horseBody_, buildHorseBody(static_cast<float>(coatLevel) / 10.0f));
        keep(horseHead_, buildHorseHead(static_cast<float>(coatLevel) / 10.0f));
        if (!horseTail_.id) keep(horseTail_, buildHorseTail());
        horseCoat_ = coatLevel;
    }
    const float dx = w.player.x - L::kHorse.x, dz = w.player.z - L::kHorse.z;
    const bool near = dx * dx + dz * dz < 36.0f;
    // The head's yaw toward the player, relative to her facing -z.
    const float toPlayer = near ? clampf(-std::atan2(dx, -dz), -0.7f, 0.7f) : 0.0f;
    const double t = seconds;
    const float headYaw = h.eatingSeconds > 0 ? 0.0f : toPlayer + 0.05f * static_cast<float>(std::sin(t * 0.7));
    const float headPitch = h.eatingSeconds > 0 ? -0.75f + 0.05f * static_cast<float>(std::sin(t * 6.0))
                                                : 0.03f * static_cast<float>(std::sin(t * 0.9));
    const float content = (1.0f - h.hunger) * (1.0f - h.dirt);
    const float swish = (0.15f + 0.35f * content) * static_cast<float>(std::sin(t * (1.5 + 2.5 * content)));
    const float lean = h.brushedSeconds > 0 ? 0.06f : 0.0f;
    horsePartTransform({0, 0, 0}, 0, 0, lean, model);
    engine_.setMeshTransform(horseBody_.id, model);
    horsePartTransform(kHorseHeadPivot, headYaw, headPitch, lean, model);
    engine_.setMeshTransform(horseHead_.id, model);
    horsePartTransform(kHorseTailPivot, swish, 0, 0, model);
    engine_.setMeshTransform(horseTail_.id, model);
}

void Scene::lights(const World& w, double seconds, float daylight, std::vector<StrataPointLight>& out) const {
    out.clear();
    int boardsOff = 0;
    for (int32_t b : w.windowBoards) boardsOff += b ? 0 : 1;
    const float off = static_cast<float>(boardsOff);
    if (daylight > 0.02f) {
        // Bounced daylight, none of it casting shadows: through the open door,
        // under each open section of the roof, and through the window as its
        // boards come off. The ruin is dim but readable, and every board pried
        // off the window brightens the room.
        const Vec3 sky{0.75f, 0.82f, 1.0f};
        out.push_back({{1.5f, 2.2f, 2.6f}, 8.0f, {sky.x, sky.y, sky.z}, 1.6f * daylight, 0});
        out.push_back({{0.0f, 2.4f, 0.6f}, 7.0f, {0.86f, 0.86f, 0.9f}, (0.8f + 0.2f * off) * daylight, 0});
        out.push_back({{-0.5f, 2.2f, -1.9f}, 5.0f, {0.86f, 0.86f, 0.9f}, (0.5f + 0.12f * off) * daylight, 0});
        const float xs[3] = {L::kRoofHoleX0, (L::kRoofHoleX0 + L::kRoofHoleX1) / 2, L::kRoofHoleX1};
        const float zs[3] = {L::kRoofHoleZ0, (L::kRoofHoleZ0 + L::kRoofHoleZ1) / 2, L::kRoofHoleZ1};
        for (int k = 0; k < 4; ++k)
            if (!((w.roofPatched >> k) & 1u))
                out.push_back({{(xs[k % 2] + xs[k % 2 + 1]) / 2, 2.6f, (zs[k / 2] + zs[k / 2 + 1]) / 2}, 4.5f,
                               {sky.x, sky.y, sky.z}, 0.35f * daylight, 0});
        if (boardsOff > 0) {
            const float wz = (L::kWindowZ0 + L::kWindowZ1) / 2;
            out.push_back({{L::kInnerX0 + 0.5f, 1.6f, wz}, 7.0f, {1.0f, 0.95f, 0.85f}, 0.6f * daylight * off, 0});
            // The patch of light on the floor under it.
            out.push_back({{L::kInnerX0 + 1.3f, 0.4f, wz}, 3.5f, {1.0f, 0.95f, 0.85f}, 0.35f * daylight * off, 0});
        }
    }
    if (daylight < 0.3f) {
        // Moonlight and starlight, faint, so a night without a fire is dark
        // but never black.
        const float night = (0.3f - daylight) / 0.3f;
        out.push_back({{0.0f, 2.4f, 0.5f}, 8.0f, {0.5f, 0.6f, 0.9f}, (0.1f + 0.03f * off) * night, 0});
    }
    const float fire = w.hearth.lit ? w.hearth.fire : 0.0f;
    if (fire > 0.01f) {
        const float f = flicker(seconds, 0.4f);
        out.push_back({{kFireBase.x, 0.55f, -2.4f}, 9.0f, {1.0f, 0.56f, 0.26f}, 5.5f * fire * f,
                       STRATA_POINT_LIGHT_CAST_SHADOWS});
        out.push_back({{kFireBase.x, 0.75f, -1.7f}, 5.5f, {1.0f, 0.6f, 0.3f}, 0.9f * fire * f, 0});
    }
    const Item* candle = placedCandle(w);
    if (candle && w.hearth.candleLit)
        out.push_back({{candle->pos.x, candle->pos.y + 0.3f, candle->pos.z + 0.08f}, 2.6f, {1.0f, 0.75f, 0.4f},
                       1.6f * flicker(seconds, 2.3f), STRATA_POINT_LIGHT_CAST_SHADOWS});
    if (w.lantern) {
        const Vec3 f = lookDirection(w.player.yaw, 0);
        out.push_back({{w.player.x + f.x * 0.3f, 1.2f, w.player.z + f.z * 0.3f}, 6.0f, {1.0f, 0.8f, 0.5f},
                       1.6f * flicker(seconds, 3.1f), 0});
    }
}

size_t Scene::meshCount() const { return byId_.size(); }

}  // namespace kindling::bridge
