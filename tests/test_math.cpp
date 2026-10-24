// The simulation's own sine, cosine and atan2 (src/sim/math.h): close to the C
// library's in every quadrant, and the look direction and boxes built on them.
#include <cmath>
#include <initializer_list>

#include "check.h"
#include "sim/math.h"

using namespace kindling;

namespace {

bool within(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

}  // namespace

int main() {
    // Sine and cosine across a few turns either way, against the C library in double.
    float worstSin = 0, worstCos = 0;
    for (int i = -4000; i <= 4000; ++i) {
        const float x = static_cast<float>(i) * 0.005f;  // -20 .. 20 radians
        const float es = static_cast<float>(std::fabs(detSin(x) - std::sin(static_cast<double>(x))));
        const float ec = static_cast<float>(std::fabs(detCos(x) - std::cos(static_cast<double>(x))));
        worstSin = es > worstSin ? es : worstSin;
        worstCos = ec > worstCos ? ec : worstCos;
    }
    CHECK(worstSin < 1e-5f);
    CHECK(worstCos < 1e-5f);

    // Within one turn the error is far smaller.
    for (int i = -1000; i <= 1000; ++i) {
        const float x = static_cast<float>(i) * kPi / 1000;
        CHECK(within(detSin(x), std::sin(static_cast<double>(x)), 1e-6));
    }

    // The exact points the rules lean on.
    CHECK(detSin(0) == 0);
    CHECK(within(detCos(0), 1, 1e-6));
    CHECK(within(detSin(kPi / 2), 1, 1e-6));
    CHECK(within(detSin(-kPi / 2), -1, 1e-6));
    CHECK(within(detCos(kPi), -1, 1e-6));
    CHECK(detSin(-1.0f) == -detSin(1.0f));

    // atan2 in all four quadrants, on both sides of the diagonal, and on the axes.
    float worstAtan = 0;
    for (int i = 0; i < 720; ++i) {
        const double a = (i - 359.5) * 3.14159265358979 / 360;  // never exactly on an axis
        for (float r : {0.01f, 1.0f, 250.0f}) {
            const float y = static_cast<float>(std::sin(a)) * r, x = static_cast<float>(std::cos(a)) * r;
            const float e = static_cast<float>(std::fabs(detAtan2(y, x) - std::atan2(static_cast<double>(y),
                                                                                      static_cast<double>(x))));
            worstAtan = e > worstAtan ? e : worstAtan;
        }
    }
    CHECK(worstAtan < 2e-5f);
    CHECK(detAtan2(0, 0) == 0);
    CHECK(within(detAtan2(0, 1), 0, 1e-6));
    CHECK(within(detAtan2(1, 0), kPi / 2, 1e-5));
    CHECK(within(detAtan2(-1, 0), -kPi / 2, 1e-5));
    CHECK(within(detAtan2(0, -1), kPi, 1e-5));
    CHECK(within(detAtan2(1, 1), kPi / 4, 1e-5));
    CHECK(within(detAtan2(-3, -3), -3 * kPi / 4, 1e-5));

    // Yaw 0 looks at the hearth (-z), positive yaw turns right (+x), pitch looks up;
    // the direction is always a unit vector.
    const Vec3 ahead = lookDirection(0, 0);
    CHECK(within(ahead.x, 0, 1e-6) && within(ahead.y, 0, 1e-6) && within(ahead.z, -1, 1e-6));
    const Vec3 right = lookDirection(kPi / 2, 0);
    CHECK(within(right.x, 1, 1e-6) && within(right.z, 0, 1e-6));
    const Vec3 up = lookDirection(0, kPi / 2);
    CHECK(within(up.y, 1, 1e-6));
    for (int i = 0; i < 64; ++i) {
        const Vec3 d = lookDirection(static_cast<float>(i) * 0.37f, static_cast<float>(i % 9 - 4) * 0.3f);
        CHECK(within(length(d), 1, 1e-5));
    }

    // Boxes include their edges and nothing past them.
    const Box2 yard{-1, -2, 1, 2};
    CHECK(yard.contains(0, 0));
    CHECK(yard.contains(1, 2));
    CHECK(yard.contains(-1, -2));
    CHECK(!yard.contains(1.01f, 0));
    CHECK(!yard.contains(0, -2.01f));
    const Box3 crate{{0, 0, 0}, {1, 1, 1}};
    CHECK(crate.contains({1, 1, 1}));
    CHECK(!crate.contains({0.5f, 1.01f, 0.5f}));
    CHECK(!crate.contains({-0.01f, 0.5f, 0.5f}));

    CHECK(clampf(5, 0, 1) == 1);
    CHECK(clampf(-5, 0, 1) == 0);
    CHECK(clampf(0.25f, 0, 1) == 0.25f);

    TEST_MAIN_END();
}
