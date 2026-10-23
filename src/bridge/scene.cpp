#include "bridge/scene.h"

#include <cmath>
#include <cstddef>

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
    if (tool_) engine_.removeMesh(tool_);
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

    float origin[3] = {eye.x, eye.y, eye.z};
    const float direction[3] = {dir.x, dir.y, dir.z};
    StrataRayHit hit = engine_.raycastMeshes(origin, direction, maxDistance);
    if (hit.mesh_id && hit.mesh_id == tool_) {
        // The tool in hand is never the target: look past it.
        for (int k = 0; k < 3; ++k) origin[k] = hit.point[k] + direction[k] * 1e-3f;
        const float skipped = hit.distance;
        hit = engine_.raycastMeshes(origin, direction, maxDistance - skipped);
        if (hit.mesh_id) hit.distance += skipped;
    }
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

    // Debris lying flat is a thin target from eye height: a ray passing close
    // to a piece counts as on it, unless something clearly nearer is in front.
    if (aim.kind == TargetKind::None || aim.kind == TargetKind::Floor || aim.kind == TargetKind::Ground ||
        aim.kind == TargetKind::Debris) {
        float nearest = aim.kind == TargetKind::Debris ? best : maxDistance;
        for (size_t i = 0; i < w.debris.size(); ++i) {
            const Debris& d = w.debris[i];
            if (d.hauled || (w.carry == Carry::Debris && w.carryIndex == static_cast<int32_t>(i))) continue;
            Vec3 a, b;
            float r;
            debrisCapsule(d, a, b, r);
            // Closest approach of the ray (eye + t dir) and the segment a..b.
            const Vec3 ab = b - a, ea = eye - a;
            const float abab = dot(ab, ab), abd = dot(ab, dir), abea = dot(ab, ea), dea = dot(dir, ea);
            const float den = abab - abd * abd;
            float u = den > 1e-8f ? clampf((abea - abd * dea) / den, 0, 1) : 0;
            float t = std::fmax(0.0f, dot(a + ab * u - eye, dir));
            u = abab > 1e-8f ? clampf(dot(eye + dir * t - a, ab) / abab, 0, 1) : 0;
            const Vec3 c = a + ab * u;
            t = std::fmax(0.0f, dot(c - eye, dir));
            const Vec3 off = eye + dir * t - c;
            if (dot(off, off) > r * r || t >= nearest || t > best + 0.35f) continue;
            nearest = t;
            aim.kind = TargetKind::Debris;
            aim.index = static_cast<int32_t>(i);
            aim.point = c;
            aim.distance = t;
        }
        if (aim.kind == TargetKind::Debris) best = std::fmin(best, aim.distance);
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

void Scene::animateTool(const World& w, double seconds, bool working) {
    const Tool tool = showPlayer_ || w.carry != Carry::Nothing ? Tool::None : w.tool;
    if (tool != toolBuilt_ || !tool_) {
        if (tool_) engine_.removeMesh(tool_);
        tool_ = 0;
        toolBuilt_ = tool;
        const MeshData m = buildTool(tool);
        if (!m.empty()) tool_ = upload(m);
    }
    if (!tool_) return;
    const Player& p = w.player;
    const Vec3 f = lookDirection(p.yaw, p.pitch);
    const Vec3 right{std::cos(p.yaw), 0, std::sin(p.yaw)};
    const Vec3 up{right.y * f.z - right.z * f.y, right.z * f.x - right.x * f.z, right.x * f.y - right.y * f.x};
    const Vec3 back = f * -1.0f;
    // The swing: side to side for the broom, brush and comb, a chop for the
    // hammer and axe, a stroke for the saw, a heave for the bar and shovel.
    const float ph = static_cast<float>(seconds * 9.0);
    float yawSwing = 0, pitchSwing = 0;
    Vec3 shift{0, 0.006f * std::sin(static_cast<float>(seconds) * 2.1f), 0};  // the hand breathing
    if (working) {
        switch (tool) {
            case Tool::Broom:
            case Tool::ScrubBrush:
            case Tool::CurryComb:
                yawSwing = 0.35f * std::sin(ph);
                shift.x += 0.05f * std::sin(ph);
                break;
            case Tool::Hammer:
            case Tool::Axe: pitchSwing = -0.7f * std::fmax(0.0f, std::sin(ph * 1.2f)); break;
            case Tool::Saw: shift.z += 0.09f * std::sin(ph * 1.4f); break;
            case Tool::PryBar:
            case Tool::Shovel: pitchSwing = 0.25f * std::sin(ph * 0.7f); break;
            case Tool::Matches: shift.y += 0.004f * std::sin(ph * 3.0f); break;
            default: break;
        }
    }
    // Columns of the local rotation (about y by yawSwing, then x by pitchSwing).
    const float cy = std::cos(yawSwing), sy = std::sin(yawSwing), cx = std::cos(pitchSwing), sx = std::sin(pitchSwing);
    const Vec3 rx{cy, 0, -sy}, ry{sy * sx, cx, cy * sx}, rz{sy * cx, -sx, cy * cx};
    auto toWorld = [&](Vec3 v) { return right * v.x + up * v.y + back * v.z; };
    const Vec3 hand = Vec3{p.x, L::kEyeHeight, p.z} + f * 0.5f + right * 0.27f + up * -0.27f + toWorld(shift);
    const Vec3 c0 = toWorld(rx), c1 = toWorld(ry), c2 = toWorld(rz);
    const float model[16] = {c0.x, c0.y, c0.z, 0, c1.x, c1.y, c1.z, 0, c2.x, c2.y, c2.z, 0, hand.x, hand.y, hand.z, 1};
    engine_.setMeshTransform(tool_, model);
}

void Scene::puff(Vec3 at, Rgb color, double seconds) {
    if (seconds - lastPuff_ < 0.06) return;
    lastPuff_ = seconds;
    auto rnd = [this] {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>(rng_ >> 8) / 16777216.0f - 0.5f;
    };
    for (int k = 0; k < 2; ++k)
        dust_.push_back({at + Vec3{rnd() * 0.3f, 0.02f, rnd() * 0.3f}, Vec3{rnd() * 0.4f, 0.25f + 0.3f * rnd(), rnd() * 0.4f},
                         color, seconds});
    if (dust_.size() > 120) dust_.erase(dust_.begin(), dust_.begin() + static_cast<std::ptrdiff_t>(dust_.size() - 120));
}

void Scene::drawDust(double seconds) {
    constexpr double kLife = 1.2;
    while (!dust_.empty() && seconds - dust_.front().born > kLife) dust_.erase(dust_.begin());
    if (dust_.empty()) return;
    std::vector<StrataPoint> points;
    points.reserve(dust_.size());
    for (const Mote& m : dust_) {
        const float age = static_cast<float>((seconds - m.born) / kLife);
        const Vec3 p = m.pos + m.vel * (age * static_cast<float>(kLife));
        StrataPoint pt{};
        pt.position[0] = p.x;
        pt.position[1] = p.y;
        pt.position[2] = p.z;
        pt.size_pixels = 2;
        pt.world_diameter = 0.12f + 0.3f * age;
        pt.color[0] = m.color.r;
        pt.color[1] = m.color.g;
        pt.color[2] = m.color.b;
        pt.color[3] = 0.6f * (1 - age);
        pt.flags = STRATA_POINT_SOFT;
        points.push_back(pt);
    }
    engine_.submitPoints(points.data(), points.size());
}

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
    else if (s.state == SettlerState::Seated) figureTransform(kChairSeatX, kChairSeatZ, s.yaw, true, model);
    else figureTransform(s.x, s.z, s.yaw, false, model);
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
