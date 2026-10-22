#include "bridge/room.h"

#include <cmath>

#include "sim/layout.h"
#include "sim/rng.h"

namespace kindling::bridge {

namespace L = kindling::layout;

namespace {

// ---- palette (linear RGB): a cold, dusty ruin
const Rgb kLog{0.33f, 0.20f, 0.10f};
const Rgb kLogEnd{0.42f, 0.30f, 0.17f};
const Rgb kPlank{0.31f, 0.19f, 0.10f};
const Rgb kRotten{0.09f, 0.07f, 0.05f};
const Rgb kNewPlank{0.46f, 0.31f, 0.17f};
const Rgb kCrawl{0.03f, 0.025f, 0.02f};
const Rgb kCeiling{0.16f, 0.10f, 0.06f};
const Rgb kBeam{0.20f, 0.12f, 0.06f};
const Rgb kShingle{0.20f, 0.17f, 0.15f};
const Rgb kMortar{0.44f, 0.41f, 0.37f};
const Rgb kSoot{0.05f, 0.04f, 0.035f};
const Rgb kAsh{0.40f, 0.40f, 0.39f};
const Rgb kHearthstone{0.28f, 0.27f, 0.25f};
const Rgb kFrame{0.20f, 0.11f, 0.055f};
const Rgb kWood{0.21f, 0.12f, 0.06f};
const Rgb kBoard{0.30f, 0.26f, 0.20f};
const Rgb kSnow{0.78f, 0.82f, 0.90f};
const Rgb kGrass{0.20f, 0.22f, 0.10f};
const Rgb kDirt{0.24f, 0.18f, 0.11f};
const Rgb kPine{0.05f, 0.11f, 0.07f};
const Rgb kCottonwood{0.62f, 0.44f, 0.10f};
const Rgb kBark{0.18f, 0.13f, 0.09f};
const Rgb kHay{0.62f, 0.50f, 0.22f};
const Rgb kCanvas{0.70f, 0.66f, 0.55f};

constexpr int kCourses = 10;
float courseY(int k) { return 0.16f + k * 0.31f; }
constexpr float kLogR = 0.17f;
constexpr float kEaveY = 3.25f, kRidgeY = 4.6f;
constexpr float kRoofX0 = -4.7f, kRoofX1 = 4.7f, kEaveZ0 = -3.7f, kEaveZ1 = 4.87f;
constexpr float kRidgeZ = 0.585f;

float backSlopeY(float z) { return kEaveY + (z - kEaveZ0) / (kRidgeZ - kEaveZ0) * (kRidgeY - kEaveY); }
float frontSlopeY(float z) { return kRidgeY - (z - kRidgeZ) / (kEaveZ1 - kRidgeZ) * (kRidgeY - kEaveY); }

// The roof hole's four sections (a 2 x 2 grid); section k is patched once
// bit k of World::roofPatched is set.
constexpr float kHoleXs[3] = {L::kRoofHoleX0, (L::kRoofHoleX0 + L::kRoofHoleX1) / 2, L::kRoofHoleX1};
constexpr float kHoleZs[3] = {L::kRoofHoleZ0, (L::kRoofHoleZ0 + L::kRoofHoleZ1) / 2, L::kRoofHoleZ1};

// One log wall: 0 back, 1 left (with the window), 2 right, 3 front (with the door).
void wall(MeshData& m, int index) {
    m.beginTag(TargetKind::Wall, index);
    for (int k = 0; k < kCourses; ++k) {
        const float y = courseY(k);
        const Rgb c = vary(kLog, static_cast<uint32_t>(k * 4 + index));
        switch (index) {
            case 0: m.logX(-4.25f, 4.25f, y, L::kInnerZ0 - kLogR, kLogR, c, kLogEnd); break;
            case 3:
                if (y < L::kDoorTop) {
                    m.logX(-4.25f, L::kDoorX0, y, L::kInnerZ1 + kLogR, kLogR, c, kLogEnd);
                    m.logX(L::kDoorX1, 4.25f, y, L::kInnerZ1 + kLogR, kLogR, c, kLogEnd);
                } else {
                    m.logX(-4.25f, 4.25f, y, L::kInnerZ1 + kLogR, kLogR, c, kLogEnd);
                }
                break;
            case 2: m.logZ(-3.3f, 4.47f, y, L::kInnerX1 + kLogR, kLogR, c, kLogEnd); break;
            default:
                if (y > L::kWindowY0 - 0.05f && y < L::kWindowY1 + 0.05f) {
                    m.logZ(-3.3f, L::kWindowZ0, y, L::kInnerX0 - kLogR, kLogR, c, kLogEnd);
                    m.logZ(L::kWindowZ1, 4.47f, y, L::kInnerX0 - kLogR, kLogR, c, kLogEnd);
                } else {
                    m.logZ(-3.3f, 4.47f, y, L::kInnerX0 - kLogR, kLogR, c, kLogEnd);
                }
        }
    }
    if (index == 3) {  // the door frame
        m.box({L::kDoorX0 - 0.1f, 0, L::kInnerZ1 - 0.02f}, {L::kDoorX0, L::kDoorTop + 0.1f, L::kInnerZ1 + 0.36f}, kFrame);
        m.box({L::kDoorX1, 0, L::kInnerZ1 - 0.02f}, {L::kDoorX1 + 0.1f, L::kDoorTop + 0.1f, L::kInnerZ1 + 0.36f}, kFrame);
        m.box({L::kDoorX0 - 0.1f, L::kDoorTop, L::kInnerZ1 - 0.02f},
              {L::kDoorX1 + 0.1f, L::kDoorTop + 0.1f, L::kInnerZ1 + 0.36f}, kFrame);
    }
    if (index == 1 || index == 2) {
        // The gable: the triangle between the top course and the roof, both faces.
        const float x = index == 1 ? L::kInnerX0 - kLogR : L::kInnerX1 + kLogR;
        for (float side : {1.0f, -1.0f}) {
            const uint32_t a = m.vertex({x, kEaveY, -3.3f}, kLog), b = m.vertex({x, kEaveY, 4.47f}, kLog);
            const uint32_t c = m.vertex({x, kRidgeY - 0.05f, kRidgeZ}, kLog);
            m.triangle(a, b, c, {side, 0, 0});
        }
    }
    m.endTag();
}

void floor(MeshData& m, const World& w) {
    for (int row = 0; row < L::kPlankRows; ++row) {
        const float z0 = L::plankZ0(row) + 0.004f, z1 = L::plankZ0(row + 1) - 0.004f;
        const Rgb c = vary(kPlank, static_cast<uint32_t>(row) * 7 + 1, 0.18f);
        int span = -1;
        for (int i = 0; i < static_cast<int>(L::kRottenSpans.size()); ++i)
            if (L::kRottenSpans[i].row == row) span = i;
        m.beginTag(TargetKind::Floor);
        if (span < 0) {
            m.box({L::kInnerX0, 0, z0}, {L::kInnerX1, L::kFloorY, z1}, c);
            m.endTag();
            continue;
        }
        const L::RottenSpan& s = L::kRottenSpans[span];
        m.box({L::kInnerX0, 0, z0}, {s.x0, L::kFloorY, z1}, c);
        m.box({s.x1, 0, z0}, {L::kInnerX1, L::kFloorY, z1}, c);
        m.endTag();
        m.beginTag(TargetKind::RottenPlank, span);
        switch (w.planks[span]) {
            case PlankState::Rotten:
                // Sagging, with a hole broken through the middle.
                m.box({s.x0, -0.01f, z0}, {s.x0 + 0.3f, L::kFloorY - 0.02f, z1}, kRotten);
                m.box({s.x1 - 0.32f, -0.01f, z0}, {s.x1, L::kFloorY - 0.02f, z1}, kRotten);
                m.box({s.x0, -0.27f, z0}, {s.x1, -0.25f, z1}, kCrawl);
                break;
            case PlankState::Removed: m.box({s.x0, -0.27f, z0}, {s.x1, -0.25f, z1}, kCrawl); break;
            case PlankState::New: m.box({s.x0, 0, z0}, {s.x1, L::kFloorY, z1}, kNewPlank); break;
        }
        m.endTag();
    }
}

void ceiling(MeshData& m, const World& w) {
    const float y0 = L::kCeilingY, y1 = L::kCeilingY + 0.1f;
    m.beginTag(TargetKind::Ceiling);
    // Around the hole.
    m.box({-4.1f, y0, -3.1f}, {L::kRoofHoleX0, y1, 4.3f}, kCeiling);
    m.box({L::kRoofHoleX1, y0, -3.1f}, {4.1f, y1, 4.3f}, kCeiling);
    m.box({L::kRoofHoleX0, y0, -3.1f}, {L::kRoofHoleX1, y1, L::kRoofHoleZ0}, kCeiling);
    m.box({L::kRoofHoleX0, y0, L::kRoofHoleZ1}, {L::kRoofHoleX1, y1, 4.3f}, kCeiling);
    for (int k = 0; k < 4; ++k) {
        if (!((w.roofPatched >> k) & 1u)) continue;
        const int i = k % 2, j = k / 2;
        m.box({kHoleXs[i], y0, kHoleZs[j]}, {kHoleXs[i + 1], y1, kHoleZs[j + 1]}, vary(kCeiling, 90 + k, 0.2f));
    }
    // Beams across the room (the middle one broke where the roof came down).
    for (float bx : {-2.4f, 2.4f}) m.box({bx - 0.13f, y0 - 0.22f, -3.1f}, {bx + 0.13f, y0, 4.3f}, kBeam);
    m.box({-0.13f, y0 - 0.22f, -3.1f}, {0.13f, y0, L::kRoofHoleZ0 + 0.1f}, kBeam);
    m.box({-0.13f, y0 - 0.22f, L::kRoofHoleZ1 + 0.15f}, {0.13f, y0, 4.3f}, kBeam);
    m.endTag();
}

// One slab of roof between z0 and z1 on a slope, x0..x1.
void roofPanel(MeshData& m, float x0, float x1, float z0, float z1, bool back, Rgb c) {
    auto y = [back](float z) { return back ? backSlopeY(z) : frontSlopeY(z); };
    const float t = 0.1f;
    const Vec3 up = back ? Vec3{0, 1, -0.32f} : Vec3{0, 1, 0.32f};
    m.quad({x0, y(z0), z0}, {x1, y(z0), z0}, {x1, y(z1), z1}, {x0, y(z1), z1}, c, up);
    m.quad({x0, y(z0) - t, z0}, {x1, y(z0) - t, z0}, {x1, y(z1) - t, z1}, {x0, y(z1) - t, z1}, c, up * -1.0f);
}

void roof(MeshData& m, const World& w) {
    // The back slope, cut into a grid whose middle four cells are the hole.
    const float xs[5] = {kRoofX0, kHoleXs[0], kHoleXs[1], kHoleXs[2], kRoofX1};
    const float zs[5] = {kEaveZ0, kHoleZs[0], kHoleZs[1], kHoleZs[2], kRidgeZ};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const bool hole = i >= 1 && i <= 2 && j >= 1 && j <= 2;
            const int section = (i - 1) + 2 * (j - 1);
            if (hole && !((w.roofPatched >> section) & 1u)) continue;
            roofPanel(m, xs[i], xs[i + 1], zs[j], zs[j + 1], true,
                      hole ? vary(kShingle, 70 + section, 0.25f) : vary(kShingle, i * 5 + j));
        }
    roofPanel(m, kRoofX0, kRoofX1, kRidgeZ, kEaveZ1, false, kShingle);
    m.box({kRoofX0, kRidgeY - 0.05f, kRidgeZ - 0.12f}, {kRoofX1, kRidgeY + 0.08f, kRidgeZ + 0.12f}, kBeam);
}

void chimney(MeshData& m) {
    m.beginTag(TargetKind::HearthFace);
    m.box({L::kBreastX0, 0, L::kBreastZ0}, {L::kFireboxX0, L::kCeilingY + 0.05f, L::kBreastFront}, kMortar);
    m.box({L::kFireboxX1, 0, L::kBreastZ0}, {L::kBreastX1, L::kCeilingY + 0.05f, L::kBreastFront}, kMortar);
    m.box({L::kFireboxX0, L::kFireboxTop, L::kBreastZ0}, {L::kFireboxX1, L::kCeilingY + 0.05f, L::kBreastFront},
          kMortar);
    // Stones proud of the mortar, in staggered courses, leaving the firebox open.
    Rng rng(1881);
    float y = 0.02f;
    int row = 0;
    while (y < L::kCeilingY) {
        const float h = rng.uniform(0.17f, 0.27f);
        float x = L::kBreastX0 + (row % 2 ? 0.12f : 0.0f) - 0.15f;
        while (x < L::kBreastX1) {
            const float wdt = rng.uniform(0.24f, 0.42f);
            const float x0 = std::fmax(x + 0.025f, L::kBreastX0), x1 = std::fmin(x + wdt - 0.025f, L::kBreastX1);
            const float y0 = y + 0.02f, y1 = std::fmin(y + h - 0.02f, L::kCeilingY);
            const bool opening = x1 > L::kFireboxX0 - 0.02f && x0 < L::kFireboxX1 + 0.02f && y0 < L::kFireboxTop;
            const float g = rng.uniform(0.30f, 0.50f), depth = rng.uniform(0.04f, 0.08f);
            if (x1 - x0 > 0.06f && y1 - y0 > 0.05f && !opening)
                m.box({x0, y0, L::kBreastFront - 0.01f}, {x1, y1, L::kBreastFront + depth},
                      {g * 1.02f, g * 0.95f, g * 0.86f});
            x += wdt;
        }
        y += h;
        ++row;
    }
    for (int k = 0; k < 7; ++k) {  // arch over the opening
        const float span = L::kFireboxX1 - L::kFireboxX0 + 0.1f;
        const float x0 = L::kFireboxX0 - 0.05f + k * span / 7;
        m.box({x0 + 0.015f, L::kFireboxTop - 0.02f, L::kBreastFront - 0.01f},
              {x0 + span / 7 - 0.015f, L::kFireboxTop + 0.2f, L::kBreastFront + 0.09f}, vary({0.42f, 0.40f, 0.36f}, k));
    }
    m.box({-2.55f, 0, L::kBreastFront}, {0.95f, 0.13f, L::kHearthstoneZ1}, kHearthstone);
    m.endTag();

    m.beginTag(TargetKind::Firebox);
    m.box({L::kFireboxX0, 0, L::kBreastZ0}, {L::kFireboxX1, L::kFireboxTop, -2.86f}, kSoot);
    m.box({L::kFireboxX0, 0, L::kBreastZ0}, {L::kFireboxX1, 0.04f, L::kBreastFront}, kSoot);
    m.endTag();

    // The chimney stack above the roof.
    m.box({-1.15f, kEaveY, -3.45f}, {-0.45f, 5.3f, -2.75f}, kMortar);
}

void mantel(MeshData& m) {
    m.beginTag(TargetKind::HearthFace);
    m.box({L::kMantelX0, L::kMantelY - 0.12f, L::kMantelZ0}, {L::kMantelX1, L::kMantelY, L::kMantelZ1}, kWood);
    m.box({-2.5f, 1.08f, -2.4f}, {-2.38f, L::kMantelY - 0.12f, -2.12f}, kWood);
    m.box({0.78f, 1.08f, -2.4f}, {0.9f, L::kMantelY - 0.12f, -2.12f}, kWood);
    m.box({-0.82f, 2.4f, -2.36f}, {-0.78f, 2.46f, -2.3f}, {0.15f, 0.15f, 0.15f});  // an empty picture hook
    m.endTag();
}

void window(MeshData& m) {
    m.beginTag(TargetKind::Wall, 1);
    m.box({-4.3f, 0.9f, -1.08f}, {-3.94f, 1.0f, 0.53f}, kFrame);  // sill
    m.box({-4.3f, 2.1f, -1.08f}, {-3.98f, 2.2f, 0.53f}, kFrame);
    m.box({-4.3f, 0.9f, -1.08f}, {-3.98f, 2.2f, -0.98f}, kFrame);
    m.box({-4.3f, 0.9f, 0.43f}, {-3.98f, 2.2f, 0.53f}, kFrame);
    m.box({-4.2f, 1.0f, -0.3f}, {-4.16f, 2.1f, -0.25f}, kFrame);  // muntins
    m.box({-4.2f, 1.52f, -1.0f}, {-4.16f, 1.57f, 0.45f}, kFrame);
    m.endTag();
}

void boards(MeshData& m, const World& w) {
    const float ys[4][2] = {{1.04f, 1.24f}, {1.33f, 1.55f}, {1.66f, 1.84f}, {1.95f, 2.12f}};
    for (int b = 0; b < 4; ++b) {
        if (!w.windowBoards[b]) continue;
        m.beginTag(TargetKind::WindowBoard, b);
        m.box({-3.98f, ys[b][0], -1.25f}, {-3.92f, ys[b][1], 0.7f}, vary(kBoard, 30 + b, 0.15f));
        m.endTag();
    }
}

void chair(MeshData& m, const World& w) {
    const float cx = 1.65f, cz = -1.15f;
    const bool mended = w.chair == ChairState::Mended;
    m.beginTag(TargetKind::Chair);
    m.box({cx - 0.3f, 0.46f, cz - 0.27f}, {cx + 0.3f, 0.52f, cz + 0.27f}, kWood);
    for (float lx : {cx - 0.26f, cx + 0.22f}) {
        for (float lz : {cz - 0.24f, cz + 0.2f}) m.box({lx, 0.1f, lz}, {lx + 0.04f, 0.48f, lz + 0.04f}, kWood);
        m.box({lx - 0.01f, 0.05f, cz - 0.5f}, {lx + 0.05f, 0.12f, cz + 0.5f}, kWood);  // rockers
        m.box({lx, 0.48f, cz + 0.22f}, {lx + 0.04f, 1.3f, cz + 0.27f}, kWood);          // back posts
        m.box({lx - 0.01f, 0.72f, cz - 0.26f}, {lx + 0.05f, 0.76f, cz + 0.27f}, kWood);  // arms
    }
    for (int k = 0; k < (mended ? 3 : 1); ++k)
        m.box({cx - 0.24f, 0.8f + k * 0.17f, cz + 0.23f}, {cx + 0.24f, 0.86f + k * 0.17f, cz + 0.26f}, kWood);
    if (mended) m.box({cx - 0.28f, 1.24f, cz + 0.22f}, {cx + 0.28f, 1.32f, cz + 0.28f}, kWood);
    m.endTag();
}

void table(MeshData& m, const World& w) {
    m.beginTag(TargetKind::Table);
    if (w.table == TableState::Toppled) {
        // On its side by the window: the top stands on edge, legs sticking out.
        m.box({-3.62f, L::kFloorY, 1.05f}, {-3.56f, 0.66f, 2.15f}, kWood);
        for (float lz : {1.12f, 2.02f})
            for (float ly : {0.12f, 0.54f}) m.box({-3.56f, ly, lz}, {-2.86f, ly + 0.05f, lz + 0.05f}, kWood);
    } else {
        m.box({-3.8f, 0.7f, 0.9f}, {-3.2f, 0.75f, 2.0f}, kWood);
        for (float lz : {0.95f, 1.9f})
            for (float lx : {-3.75f, -3.3f}) m.box({lx, 0, lz}, {lx + 0.05f, 0.7f, lz + 0.05f}, kWood);
    }
    m.endTag();
}

void snow(MeshData& m, const World& w) {
    if (w.snow <= 0) return;
    const float s = w.snow;
    m.beginTag(TargetKind::Snow);
    m.ellipsoid({0.1f, L::kFloorY, -0.9f}, {0.45f + 0.35f * s, 0.05f + 0.12f * s, 0.35f + 0.3f * s}, kSnow);
    if (s > 0.4f) m.ellipsoid({0.35f, L::kFloorY, -0.6f}, {0.45f * s, 0.1f * s, 0.4f * s}, kSnow);
    m.endTag();
}

void pine(MeshData& m, float x, float z, float h) {
    m.rod({x, 0, z}, {x, h * 0.15f, z}, h * 0.03f, kBark, 6);
    for (int i = 0; i < 3; ++i) {
        const float wdt = h * (0.42f - i * 0.1f), b = h * (0.1f + i * 0.24f);
        m.cone({x, b, z}, wdt / 2, h * 0.42f, kPine, {0.07f, 0.14f, 0.09f}, 8);
    }
}

void cottonwood(MeshData& m, float x, float z, float h) {
    m.rod({x, 0, z}, {x, h * 0.6f, z}, h * 0.045f, kBark, 7);
    m.ellipsoid({x, h * 0.78f, z}, {h * 0.32f, h * 0.26f, h * 0.32f}, kCottonwood, 10, 6);
    m.ellipsoid({x + h * 0.18f, h * 0.66f, z + 0.1f}, {h * 0.2f, h * 0.17f, h * 0.2f}, vary(kCottonwood, 5), 8, 5);
}

void yard(MeshData& m) {
    // The wagon: a bed on four wheels, a canvas cover over the back half.
    m.beginTag(TargetKind::Wagon);
    m.box({1.3f, 0.7f, 7.1f}, {3.7f, 0.82f, 9.3f}, kWood);
    m.box({1.3f, 0.82f, 7.1f}, {1.38f, 1.2f, 9.3f}, kWood);
    m.box({3.62f, 0.82f, 7.1f}, {3.7f, 1.2f, 9.3f}, kWood);
    m.box({1.3f, 0.82f, 9.22f}, {3.7f, 1.2f, 9.3f}, kWood);
    for (float wz : {7.4f, 9.0f})
        for (float wx : {1.22f, 3.78f}) m.logX(wx - 0.06f, wx + 0.06f, 0.42f, wz, 0.42f, kWood, kBark, 14);
    m.ellipsoid({2.5f, 1.2f, 8.7f}, {1.25f, 0.85f, 0.7f}, kCanvas, 12, 6);
    m.rod({2.5f, 0.6f, 7.1f}, {2.5f, 0.45f, 5.6f}, 0.05f, kWood, 6);  // the tongue
    m.endTag();

    m.beginTag(TargetKind::ChoppingBlock);
    m.rod({-1.8f, 0, 7.0f}, {-1.8f, 0.55f, 7.0f}, 0.33f, {0.36f, 0.26f, 0.16f}, 12);
    m.endTag();

    m.beginTag(TargetKind::LogPile);
    for (int row = 0; row < 3; ++row)
        for (int k = 0; k < 4 - row; ++k)
            m.logZ(7.45f, 8.25f, 0.17f + row * 0.3f, -3.4f + k * 0.3f + row * 0.15f, 0.15f, vary(kBark, k + 9 * row),
                   {0.55f, 0.40f, 0.24f}, 10);
    m.endTag();

    m.beginTag(TargetKind::HayBale);
    m.box({5.0f, 0, 7.9f}, {5.8f, 0.55f, 8.6f}, kHay);
    m.endTag();

    m.beginTag(TargetKind::Stall);
    for (float px : {6.1f, 7.6f, 9.1f}) m.box({px - 0.07f, 0, 11.03f}, {px + 0.07f, 1.4f, 11.17f}, kWood);
    for (float pz : {7.5f, 9.25f, 11.1f}) {
        m.box({6.03f, 0, pz - 0.07f}, {6.17f, 1.4f, pz + 0.07f}, kWood);
        m.box({9.03f, 0, pz - 0.07f}, {9.17f, 1.4f, pz + 0.07f}, kWood);
    }
    for (float ry : {0.6f, 1.15f}) {
        m.box({6.0f, ry, 11.05f}, {9.2f, ry + 0.1f, 11.15f}, kWood);
        m.box({6.05f, ry, 7.4f}, {6.15f, ry + 0.1f, 11.2f}, kWood);
        m.box({9.05f, ry, 7.4f}, {9.15f, ry + 0.1f, 11.2f}, kWood);
    }
    m.box({6.0f, 1.9f, 7.2f}, {9.2f, 2.0f, 11.3f}, kShingle);  // a lean-to roof
    m.endTag();

    // Trees: pines on the ridge behind, cottonwoods along the creek in front.
    Rng rng(42);
    for (int k = 0; k < 26; ++k) {
        const float x = rng.uniform(-30, 30), z = rng.uniform(-30, -9), h = rng.uniform(5, 11);
        pine(m, x, z, h);
    }
    for (int k = 0; k < 8; ++k) {
        const float x = -24 + k * 6.5f + rng.uniform(-1.5f, 1.5f), z = rng.uniform(21, 25), h = rng.uniform(6, 10);
        cottonwood(m, x, z, h);
    }
}

void ground(MeshData& m, const World& w) {
    m.beginTag(TargetKind::Ground);
    const Rgb grass = frosted(w) ? Rgb{0.52f, 0.56f, 0.58f} : kGrass;
    m.quad({-60, 0, -60}, {60, 0, -60}, {60, 0, 60}, {-60, 0, 60}, grass, {0, 1, 0});
    // A worn path from the door to the wagon, just above the grass.
    m.quad({2.0f, 0.005f, 4.4f}, {3.0f, 0.005f, 4.4f}, {3.4f, 0.005f, 7.0f}, {1.8f, 0.005f, 7.0f}, kDirt, {0, 1, 0});
    m.endTag();
}

const Rgb kSplit{0.55f, 0.40f, 0.24f};
const Rgb kRoundBark{0.20f, 0.14f, 0.09f};

void fireboxContents(MeshData& m, const World& w) {
    const Hearth& h = w.hearth;
    const float cx = (L::kFireboxX0 + L::kFireboxX1) / 2;
    m.beginTag(TargetKind::Firebox);
    // Andirons, always.
    m.box({cx - 0.48f, 0.04f, -2.7f}, {cx - 0.42f, 0.24f, -2.45f}, {0.06f, 0.06f, 0.06f});
    m.box({cx + 0.42f, 0.04f, -2.7f}, {cx + 0.48f, 0.24f, -2.45f}, {0.06f, 0.06f, 0.06f});
    if (h.ash > 0) {
        m.box({cx - 0.55f, 0.04f, -2.82f}, {cx + 0.55f, 0.04f + 0.05f * h.ash, -2.4f}, kAsh);
        m.logX(cx - 0.35f, cx + 0.2f, 0.1f, -2.6f, 0.06f, {0.06f, 0.06f, 0.06f}, {0.18f, 0.18f, 0.18f});
    }
    if (h.kindlingLaid)
        for (int k = 0; k < 7; ++k)
            m.box({cx - 0.3f + 0.09f * k, 0.05f, -2.66f + 0.03f * (k % 3)}, {cx - 0.27f + 0.09f * k, 0.09f, -2.5f + 0.03f * (k % 3)},
                  vary({0.55f, 0.40f, 0.22f}, k));
    const int logs = h.lit ? (h.fuelMinutes > 240 ? 3 : h.fuelMinutes > 0 ? 2 : 0) : h.logsLaid;
    const Rgb charred = h.lit ? Rgb{0.12f, 0.07f, 0.04f} : kSplit;
    for (int k = 0; k < logs; ++k)
        m.logX(cx - 0.5f + 0.05f * k, cx + 0.45f - 0.05f * k, 0.2f + 0.12f * (k == 2), -2.62f + 0.14f * (k == 1), 0.075f,
               charred, {0.5f, 0.3f, 0.12f});
    if (h.lit) m.box({cx - 0.5f, 0.04f, -2.78f}, {cx + 0.5f, 0.07f, -2.42f}, {0.35f, 0.12f, 0.04f});  // embers bed
    m.endTag();
}

void yardWood(MeshData& m, const World& w) {
    const Firewood& f = w.wood;
    m.beginTag(TargetKind::LogPile);
    for (int k = 0; k < f.rounds; ++k) {
        const int row = k < 3 ? 0 : k < 5 ? 1 : 2, inRow = k < 3 ? k : k < 5 ? k - 3 : 0;
        const float x = -3.35f + inRow * 0.32f + row * 0.16f, y = 0.0f + row * 0.36f;
        m.rod({x, y, 7.85f}, {x, y + 0.36f, 7.85f}, 0.15f, kRoundBark, 10);
    }
    m.endTag();
    if (f.roundOnBlock) {
        m.beginTag(TargetKind::ChoppingBlock);
        m.rod({-1.8f, 0.55f, 7.0f}, {-1.8f, 0.9f, 7.0f}, 0.15f, kRoundBark, 10);
        m.endTag();
    }
    m.beginTag(TargetKind::SplitWood);
    for (int k = 0; k < f.splitByBlock && k < 12; ++k)
        m.logZ(6.2f + 0.02f * k, 6.75f, 0.06f + 0.11f * (k / 4), -1.25f + 0.13f * (k % 4), 0.06f, kSplit, {0.62f, 0.45f, 0.26f}, 6);
    m.endTag();
    m.beginTag(TargetKind::Kindling);
    for (int k = 0; k < f.kindlingByBlock && k < 6; ++k)
        for (int s = 0; s < 6; ++s)
            m.box({-2.45f + 0.05f * s + 0.02f * k, 0.02f * k, 6.3f + 0.04f * k}, {-2.43f + 0.05f * s + 0.02f * k, 0.02f * k + 0.03f, 6.75f},
                  vary({0.55f, 0.40f, 0.22f}, s + 6 * k));
    m.endTag();
}

void woodpile(MeshData& m, const World& w) {
    m.beginTag(TargetKind::Woodpile);
    // A low frame of two rails, always, so the spot is findable when empty.
    m.box({-3.62f, 0.0f, -2.75f}, {-2.62f, 0.05f, -2.7f}, kWood);
    m.box({-3.62f, 0.0f, -2.1f}, {-2.62f, 0.05f, -2.05f}, kWood);
    const int counts[4] = {5, 4, 3, 2};
    int left = w.wood.woodpile;
    for (int row = 0; row < 4 && left > 0; ++row)
        for (int k = 0; k < counts[row] && left > 0; ++k, --left) {
            const float x = -3.5f + 0.17f * k + 0.085f * row;
            m.logZ(-2.75f, -2.05f, 0.13f + 0.15f * row, x, 0.08f, vary(kSplit, 50 + row * 5 + k), {0.62f, 0.45f, 0.26f}, 8);
        }
    m.endTag();
}

}  // namespace

MeshData buildFlames() {
    MeshData m;
    const Rgb outer{1.0f, 0.36f, 0.05f}, outerTip{1.0f, 0.62f, 0.12f}, core{1.0f, 0.75f, 0.3f}, coreTip{1.0f, 0.95f, 0.7f};
    const float tongues[5][3] = {{-0.3f, 0.42f, 0.13f}, {-0.12f, 0.62f, 0.16f}, {0.08f, 0.55f, 0.15f},
                                 {0.26f, 0.4f, 0.12f}, {0.0f, 0.3f, 0.2f}};
    for (int k = 0; k < 5; ++k)
        m.cone({tongues[k][0], 0, 0.05f * (k % 2)}, tongues[k][2], tongues[k][1], outer, outerTip);
    const float cores[3][3] = {{-0.12f, 0.38f, 0.08f}, {0.08f, 0.34f, 0.08f}, {-0.02f, 0.24f, 0.12f}};
    for (const auto& c : cores) m.cone({c[0], 0.02f, 0.06f}, c[2], c[1], core, coreTip);
    return m;
}

MeshData buildCandleFlame() {
    MeshData m;
    m.cone({0, 0, 0}, 0.018f, 0.07f, {1.0f, 0.7f, 0.25f}, {1.0f, 0.95f, 0.75f}, 8);
    return m;
}

MeshData buildSettler() {
    MeshData m;
    const Rgb dress{0.32f, 0.12f, 0.30f}, shawl{0.62f, 0.52f, 0.36f}, skin{0.78f, 0.56f, 0.44f}, hair{0.18f, 0.10f, 0.06f};
    m.cone({0, 0, 0}, 0.3f, 1.05f, dress, dress, 14);           // skirt
    m.rod({0, 0.85f, 0}, {0, 1.32f, 0}, 0.15f, dress, 12);       // bodice
    m.ellipsoid({0, 1.25f, 0}, {0.21f, 0.1f, 0.17f}, shawl, 12, 6);
    m.rod({0, 1.32f, 0}, {0, 1.4f, 0}, 0.05f, skin, 8);           // neck
    m.ellipsoid({0, 1.5f, 0}, {0.1f, 0.12f, 0.11f}, skin, 12, 8);
    m.ellipsoid({0, 1.56f, 0.06f}, {0.09f, 0.08f, 0.07f}, hair, 10, 6);  // her hair, put up behind
    m.ellipsoid({0, 1.6f, 0.11f}, {0.05f, 0.05f, 0.05f}, hair, 8, 5);
    for (float side : {-1.0f, 1.0f})
        m.rod({side * 0.17f, 1.25f, 0}, {side * 0.2f, 0.9f, -0.05f}, 0.045f, shawl, 8);  // arms
    return m;
}

MeshData buildHomesteader() {
    MeshData m;
    const Rgb trousers{0.20f, 0.18f, 0.16f}, coat{0.36f, 0.24f, 0.14f}, skin{0.76f, 0.55f, 0.42f}, hat{0.16f, 0.12f, 0.08f};
    for (float side : {-1.0f, 1.0f}) m.rod({side * 0.1f, 0, 0}, {side * 0.1f, 0.85f, 0}, 0.07f, trousers, 8);
    m.rod({0, 0.8f, 0}, {0, 1.4f, 0}, 0.19f, coat, 12);
    for (float side : {-1.0f, 1.0f}) m.rod({side * 0.22f, 1.35f, 0}, {side * 0.25f, 0.85f, -0.05f}, 0.055f, coat, 8);
    m.ellipsoid({0, 1.55f, 0}, {0.1f, 0.12f, 0.11f}, skin, 12, 8);
    m.rod({0, 1.62f, 0}, {0, 1.64f, 0}, 0.2f, hat, 14);   // brim
    m.rod({0, 1.64f, 0}, {0, 1.76f, 0}, 0.11f, hat, 12);  // crown
    return m;
}

namespace {

Rgb coat(float dirt) {
    const Rgb clean{0.32f, 0.15f, 0.07f}, dusty{0.40f, 0.33f, 0.25f};
    return {clean.r + (dusty.r - clean.r) * dirt, clean.g + (dusty.g - clean.g) * dirt, clean.b + (dusty.b - clean.b) * dirt};
}

}  // namespace

MeshData buildHorseBody(float dirt) {
    MeshData m;
    m.beginTag(TargetKind::Horse);
    const Rgb c = coat(dirt), hoof{0.08f, 0.07f, 0.06f};
    m.ellipsoid({0, 1.25f, 0}, {0.32f, 0.36f, 0.85f}, c, 16, 10);
    for (float sx : {-0.2f, 0.2f})
        for (float sz : {-0.55f, 0.55f}) {
            m.rod({sx, 1.05f, sz}, {sx, 0.12f, sz}, 0.065f, c, 8);
            m.rod({sx, 0.12f, sz}, {sx, 0, sz}, 0.08f, hoof, 8);
        }
    m.endTag();
    return m;
}

MeshData buildHorseHead(float dirt) {
    MeshData m;
    m.beginTag(TargetKind::Horse);
    const Rgb c = coat(dirt), mane{0.10f, 0.06f, 0.04f}, blaze{0.85f, 0.82f, 0.76f};
    const Vec3 p = kHorseHeadPivot;
    m.rod(Vec3{0, 1.45f, -0.7f} - p, Vec3{0, 1.95f, -1.05f} - p, 0.14f, c, 10);           // neck
    m.ellipsoid(Vec3{0, 1.88f, -1.28f} - p, {0.11f, 0.13f, 0.3f}, c, 12, 8);               // head
    m.box(Vec3{-0.02f, 1.98f, -1.55f} - p, Vec3{0.02f, 2.0f, -1.2f} - p, blaze);           // a white blaze
    m.box(Vec3{-0.03f, 1.6f, -0.95f} - p, Vec3{0.03f, 2.1f, -0.65f} - p, mane);            // mane
    for (float sx : {-0.06f, 0.06f}) m.cone(Vec3{sx, 2.0f, -1.08f} - p, 0.035f, 0.12f, c, c, 6);  // ears
    m.endTag();
    return m;
}

MeshData buildHorseTail() {
    MeshData m;
    m.beginTag(TargetKind::Horse);
    m.rod({0, 0, 0}, Vec3{0, 0.7f, 1.12f} - kHorseTailPivot, 0.06f, {0.10f, 0.06f, 0.04f}, 8);
    m.endTag();
    return m;
}

void horsePartTransform(Vec3 pivot, float yaw, float pitch, float roll, float out[16]) {
    // M = T(stall) * T(pivot) * Ry(yaw) * Rx(pitch) * Rz(roll), the mare facing -z.
    const float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const float cr = std::cos(roll), sr = std::sin(roll);
    // Rx * Rz
    const float a[9] = {cr, cp * sr, sp * sr, -sr, cp * cr, sp * cr, 0, -sp, cp};  // columns
    // Ry * (Rx * Rz): Ry columns {cy,0,-sy},{0,1,0},{sy,0,cy}
    float r[9];
    for (int col = 0; col < 3; ++col) {
        const float x = a[col * 3], y = a[col * 3 + 1], z = a[col * 3 + 2];
        r[col * 3] = cy * x + sy * z;
        r[col * 3 + 1] = y;
        r[col * 3 + 2] = -sy * x + cy * z;
    }
    const Vec3 t{layout::kHorse.x + pivot.x, pivot.y, layout::kHorse.z + pivot.z};
    const float m[16] = {r[0], r[1], r[2], 0, r[3], r[4], r[5], 0, r[6], r[7], r[8], 0, t.x, t.y, t.z, 1};
    for (int i = 0; i < 16; ++i) out[i] = m[i];
}

void figureTransform(float x, float z, float yaw, bool seated, float out[16]) {
    // Yaw as the player's: 0 faces -z, positive turns toward +x.
    const float c = std::cos(-yaw), s = std::sin(-yaw);
    const float y = seated ? -0.42f : 0.0f;
    const float m[16] = {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, x, y, z, 1};
    for (int i = 0; i < 16; ++i) out[i] = m[i];
}

MeshData buildPart(Part part, const World& w) {
    MeshData m;
    switch (part) {
        case Part::WallBack: wall(m, 0); break;
        case Part::WallLeft: wall(m, 1); break;
        case Part::WallRight: wall(m, 2); break;
        case Part::WallFront: wall(m, 3); break;
        case Part::Floor: floor(m, w); break;
        case Part::Ceiling: ceiling(m, w); break;
        case Part::Roof: roof(m, w); break;
        case Part::Chimney: chimney(m); break;
        case Part::Mantel: mantel(m); break;
        case Part::Window: window(m); break;
        case Part::Boards: boards(m, w); break;
        case Part::Chair: chair(m, w); break;
        case Part::Table: table(m, w); break;
        case Part::Snow: snow(m, w); break;
        case Part::Yard: yard(m); break;
        case Part::Ground: ground(m, w); break;
        case Part::FireboxContents: fireboxContents(m, w); break;
        case Part::YardWood: yardWood(m, w); break;
        case Part::Woodpile: woodpile(m, w); break;
        case Part::Count: break;
    }
    Surface surface;
    if (partSurface(part, surface)) {
        // Planar uvs onto the grime surface, clamped so the parts of a mesh off
        // the surface (log ends, the gable) take its edge texels.
        const SurfaceDef& d = surfaceDef(surface);
        m.uvs.resize(m.vertexCount() * 2);
        for (uint32_t i = 0; i < m.vertexCount(); ++i) {
            const Vec3 p{m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]};
            const Vec3 r = p - d.origin;
            m.uvs[2 * i] = std::fmin(1.0f, std::fmax(0.0f, dot(r, d.u) / d.uLength));
            m.uvs[2 * i + 1] = std::fmin(1.0f, std::fmax(0.0f, dot(r, d.v) / d.vLength));
        }
        m.wantUv = true;
    }
    return m;
}

bool partSurface(Part part, Surface& out) {
    switch (part) {
        case Part::Floor: out = Surface::Floor; return true;
        case Part::WallBack: out = Surface::BackWall; return true;
        case Part::WallLeft: out = Surface::LeftWall; return true;
        case Part::WallRight: out = Surface::RightWall; return true;
        case Part::WallFront: out = Surface::FrontWall; return true;
        case Part::Chimney: out = Surface::HearthFace; return true;
        default: return false;
    }
}

uint64_t partVersion(Part part, const World& w) {
    switch (part) {
        case Part::Floor:
            return static_cast<uint64_t>(w.planks[0]) | static_cast<uint64_t>(w.planks[1]) << 4 |
                   static_cast<uint64_t>(w.planks[2]) << 8;
        case Part::Ceiling:
        case Part::Roof: return static_cast<uint64_t>(w.roofPatched);
        case Part::Boards: {
            uint64_t v = 0;
            for (int b = 0; b < 4; ++b) v |= static_cast<uint64_t>(w.windowBoards[b] != 0) << b;
            return v;
        }
        case Part::Chair: return static_cast<uint64_t>(w.chair);
        case Part::Table: return static_cast<uint64_t>(w.table);
        case Part::Snow: return static_cast<uint64_t>(std::lround(w.snow * 20));
        case Part::Ground: return frosted(w) ? 1 : 0;
        case Part::FireboxContents: {
            const Hearth& h = w.hearth;
            const uint64_t logs = h.lit ? (h.fuelMinutes > 240 ? 3u : h.fuelMinutes > 0 ? 2u : 0u) : static_cast<uint64_t>(h.logsLaid);
            return static_cast<uint64_t>(std::lround(h.ash * 10)) | static_cast<uint64_t>(h.kindlingLaid) << 8 | logs << 9 |
                   static_cast<uint64_t>(h.lit) << 12;
        }
        case Part::YardWood:
            return static_cast<uint64_t>(w.wood.rounds) | static_cast<uint64_t>(w.wood.roundOnBlock) << 8 |
                   static_cast<uint64_t>(w.wood.splitByBlock) << 9 | static_cast<uint64_t>(w.wood.kindlingByBlock) << 20;
        case Part::Woodpile: return static_cast<uint64_t>(w.wood.woodpile);
        default: return 0;
    }
}

MeshData buildDebris(const Debris& d, int32_t index) {
    MeshData m;
    m.beginTag(TargetKind::Debris, index);
    switch (d.kind) {
        case DebrisKind::Board: {
            const float len = 0.5f + 0.05f * static_cast<float>(d.id % 8);
            m.box({-len / 2, -0.025f, -0.06f}, {len / 2, 0.025f, 0.06f}, vary(kBoard, static_cast<uint32_t>(d.id), 0.2f));
            break;
        }
        case DebrisKind::Shingle:
            m.box({-0.15f, -0.012f, -0.08f}, {0.15f, 0.012f, 0.08f}, vary(kShingle, static_cast<uint32_t>(d.id), 0.2f));
            break;
        case DebrisKind::Rafter: m.rod({0, 0, 0}, {1.97f, 2.75f, 0}, 0.11f, kBeam, 10); break;
        case DebrisKind::RafterPiece: m.rod({-0.37f, 0, 0}, {0.37f, 0, 0}, 0.11f, kBeam, 10); break;
    }
    m.endTag();
    return m;
}

MeshData buildItem(const Item& item, int32_t index) {
    MeshData m;
    m.beginTag(TargetKind::Item, index);
    const Rgb wicker{0.45f, 0.31f, 0.15f}, cream{0.82f, 0.76f, 0.60f}, red{0.40f, 0.10f, 0.07f};
    switch (item.kind) {
        case ItemKind::Plank: m.box({-0.5f, 0, -0.09f}, {0.5f, 0.05f, 0.09f}, kNewPlank); break;
        case ItemKind::Shingles:
            for (int k = 0; k < 4; ++k)
                m.box({-0.2f, 0.03f * k, -0.15f}, {0.2f, 0.03f * k + 0.025f, 0.15f}, vary({0.30f, 0.24f, 0.18f}, k, 0.2f));
            m.box({-0.02f, 0, -0.16f}, {0.02f, 0.13f, 0.16f}, {0.55f, 0.47f, 0.30f});  // twine
            break;
        case ItemKind::Rug: {
            const float rings[5][2] = {{1.4f, 0.9f}, {1.15f, 0.7f}, {0.95f, 0.53f}, {0.65f, 0.33f}, {0.4f, 0.15f}};
            const Rgb colours[5] = {{0.30f, 0.09f, 0.06f}, {0.45f, 0.34f, 0.18f}, {0.26f, 0.08f, 0.055f},
                                    {0.22f, 0.28f, 0.40f}, {0.45f, 0.34f, 0.18f}};
            for (int k = 0; k < 5; ++k) m.disc({0, 0.006f + 0.002f * k, 0}, rings[k][0], rings[k][1], colours[k]);
            break;
        }
        case ItemKind::Quilt: {
            const Rgb patches[5] = {{0.55f, 0.14f, 0.12f}, {0.85f, 0.75f, 0.55f}, {0.20f, 0.32f, 0.50f},
                                    {0.70f, 0.50f, 0.20f}, {0.30f, 0.45f, 0.30f}};
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 4; ++j)
                    m.box({-0.3f + 0.1f * i, -0.26f + 0.13f * j, -0.015f}, {-0.2f + 0.1f * i, -0.13f + 0.13f * j, 0.015f},
                          patches[(i + 2 * j) % 5]);
            break;
        }
        case ItemKind::Curtains:
            // Hung either side of the window (on the left wall, so along z).
            m.box({0.02f, -1.3f, -1.175f}, {0.08f, 0.05f, -0.705f}, red);
            m.box({0.02f, -1.3f, 0.705f}, {0.08f, 0.05f, 1.175f}, red);
            m.box({0.0f, 0.02f, -1.3f}, {0.07f, 0.08f, 1.3f}, {0.12f, 0.07f, 0.04f});  // the rod
            break;
        case ItemKind::Sampler:
            m.box({-0.52f, -0.36f, -0.05f}, {0.52f, 0.36f, 0.0f}, {0.17f, 0.09f, 0.04f});
            m.box({-0.44f, -0.28f, 0.0f}, {0.44f, 0.28f, 0.02f}, {0.78f, 0.72f, 0.58f});
            for (int k = 0; k < 5; ++k)
                m.box({-0.3f + 0.16f * k, 0.0f, 0.02f}, {-0.2f + 0.16f * k, 0.12f, 0.03f},
                      k % 2 ? Rgb{0.18f, 0.35f, 0.22f} : Rgb{0.6f, 0.12f, 0.1f});
            m.box({-0.32f, -0.12f, 0.02f}, {0.28f, -0.08f, 0.03f}, {0.25f, 0.30f, 0.45f});
            break;
        case ItemKind::Crock: m.rod({0, 0, 0}, {0, 0.26f, 0}, 0.08f, {0.55f, 0.48f, 0.32f}, 14); break;
        case ItemKind::Jar: m.rod({0, 0, 0}, {0, 0.2f, 0}, 0.06f, {0.25f, 0.40f, 0.50f}, 14); break;
        case ItemKind::Clock:
            m.box({-0.15f, 0, -0.07f}, {0.15f, 0.32f, 0.07f}, {0.25f, 0.13f, 0.06f});
            m.rod({0, 0.2f, 0.07f}, {0, 0.2f, 0.08f}, 0.09f, {0.85f, 0.8f, 0.65f}, 16);
            break;
        case ItemKind::Candle:
            m.rod({0, 0, 0}, {0, 0.01f, 0}, 0.08f, {0.45f, 0.38f, 0.22f}, 12);
            m.rod({0, 0.01f, 0}, {0, 0.23f, 0}, 0.04f, cream, 10);
            break;
        case ItemKind::Basket:
            m.rod({0, 0, 0}, {0, 0.36f, 0}, 0.21f, wicker, 14);
            for (int k = 0; k < 9; ++k) {
                const float x = 0.12f * std::cos(k * 2.1f), z = 0.12f * std::sin(k * 1.7f);
                m.box({x - 0.015f, 0.36f, z - 0.015f}, {x + 0.015f, 0.5f + 0.02f * (k % 4), z + 0.015f},
                      vary({0.55f, 0.40f, 0.22f}, k));
            }
            break;
        case ItemKind::Count: break;
    }
    m.endTag();
    return m;
}

void itemTransform(const Item& item, float out[16]) {
    // Big things ride folded or rolled while carried, so they do not fill the view.
    const bool big = item.kind == ItemKind::Rug || item.kind == ItemKind::Curtains || item.kind == ItemKind::Sampler;
    const float k = item.place == ItemPlace::Carried && big ? 0.3f : 1.0f;
    const float c = std::cos(item.yaw) * k, s = std::sin(item.yaw) * k;
    const float m[16] = {c, 0, -s, 0, 0, k, 0, 0, s, 0, c, 0, item.pos.x, item.pos.y, item.pos.z, 1};
    for (int i = 0; i < 16; ++i) out[i] = m[i];
}

void itemFootprint(ItemKind kind, float& halfX, float& halfZ) {
    switch (kind) {
        case ItemKind::Rug: halfX = 1.4f; halfZ = 0.9f; break;
        case ItemKind::Basket: halfX = halfZ = 0.22f; break;
        case ItemKind::Sampler: halfX = 0.52f; halfZ = 0.36f; break;
        case ItemKind::Curtains: halfX = 0.1f; halfZ = 1.2f; break;
        case ItemKind::Quilt: halfX = 0.3f; halfZ = 0.3f; break;
        default: halfX = halfZ = 0.12f; break;
    }
}

void debrisTransform(const Debris& d, float out[16]) {
    const float c = std::cos(d.yaw), s = std::sin(d.yaw);
    const float m[16] = {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, d.pos.x, d.pos.y, d.pos.z, 1};
    for (int i = 0; i < 16; ++i) out[i] = m[i];
}

}  // namespace kindling::bridge
