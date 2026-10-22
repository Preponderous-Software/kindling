#include "bridge/scene.h"

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

void Scene::updateGrime(const World& w, const std::array<TexelRect, kSurfaceCount>& changed) {
    for (int s = 0; s < kSurfaceCount; ++s)
        if (changed[static_cast<size_t>(s)].w) uploadGrime(w, static_cast<Surface>(s), changed[static_cast<size_t>(s)]);
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
    if (dir.y > 1e-4f && w.roofPatches < 4) {
        const float t = (L::kCeilingY - eye.y) / dir.y;
        const Vec3 p = eye + dir * t;
        if (t > 0 && t < best && p.x > L::kRoofHoleX0 && p.x < L::kRoofHoleX1 && p.z > L::kRoofHoleZ0 &&
            p.z < L::kRoofHoleZ1) {
            const int i = p.x < (L::kRoofHoleX0 + L::kRoofHoleX1) / 2 ? 0 : 1;
            const int j = p.z < (L::kRoofHoleZ0 + L::kRoofHoleZ1) / 2 ? 0 : 1;
            aim.kind = TargetKind::RoofHole;
            aim.index = i + 2 * j;
            aim.point = p;
            aim.distance = t;
        }
    }
    return aim;
}

size_t Scene::meshCount() const { return byId_.size(); }

}  // namespace kindling::bridge
