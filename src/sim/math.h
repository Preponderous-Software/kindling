// Small vector helpers for the simulation. Plain floats only: the rules are
// compiled without fused multiply-adds so results are identical on every
// compiler (see CMakeLists.txt).
#pragma once

#include <cmath>

namespace kindling {

struct Vec2 {
    float x = 0, z = 0;
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }

// An axis-aligned box on the ground plane (x, z), used for collision and for
// "is the player inside this area" tests.
struct Box2 {
    float x0 = 0, z0 = 0, x1 = 0, z1 = 0;
    bool contains(float x, float z) const { return x >= x0 && x <= x1 && z >= z0 && z <= z1; }
};

// An axis-aligned box in 3D, used for aim targets that have no mesh of their own.
struct Box3 {
    Vec3 lo, hi;
    bool contains(Vec3 p) const {
        return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y && p.z >= lo.z && p.z <= hi.z;
    }
};

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

constexpr float kPi = 3.14159265358979f;

// Sine and cosine from + - * only, so every compiler gets the same bits (the
// C library's sin and cos may differ in the last bit between platforms, which
// would break the pinned fingerprints). Accurate to about 1e-7 in [-pi, pi].
inline float detSin(float x) {
    x = x - 2 * kPi * std::nearbyint(x / (2 * kPi));  // to [-pi, pi]
    if (x > kPi / 2) x = kPi - x;
    else if (x < -kPi / 2) x = -kPi - x;              // to [-pi/2, pi/2]
    const float x2 = x * x;
    return x * (1 + x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040 + x2 * (1.0f / 362880 +
                                                                                  x2 * (-1.0f / 39916800))))));
}
inline float detCos(float x) { return detSin(x + kPi / 2); }

// atan2 from + - * / only (see detSin), accurate to about 1e-5 radians.
inline float detAtan2(float y, float x) {
    if (x == 0 && y == 0) return 0;
    const float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    const bool swap = ay > ax;
    const float t = swap ? ax / ay : ay / ax;  // 0..1
    const float t2 = t * t;
    // Minimax polynomial for atan on [0, 1].
    float a = t * (0.99997726f + t2 * (-0.33262347f + t2 * (0.19354346f + t2 * (-0.11643287f + t2 * (0.05265332f +
                                                                                                  t2 * -0.01172120f)))));
    if (swap) a = kPi / 2 - a;
    if (x < 0) a = kPi - a;
    return y < 0 ? -a : a;
}

// The direction the player looks: yaw 0 faces -z (toward the hearth), positive yaw turns right.
inline Vec3 lookDirection(float yaw, float pitch) {
    const float cp = detCos(pitch);
    return {detSin(yaw) * cp, detSin(pitch), -detCos(yaw) * cp};
}

}  // namespace kindling
