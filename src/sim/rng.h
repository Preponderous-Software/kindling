// PCG32 (O'Neill), the same generator Strata's strata::sim uses: small, fast
// and identical on every compiler. Used only while building a new world, so
// the sequence never has to be saved.
#pragma once

#include <cstdint>

namespace kindling {

class Rng {
public:
    explicit Rng(uint64_t seed) {
        state_ = 0;
        next();
        state_ += seed;
        next();
    }
    uint32_t next() {
        const uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + 1442695040888963407ULL;
        const auto xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }
    // A float in [lo, hi), from 24 random bits so it is exact.
    float uniform(float lo, float hi) { return lo + (hi - lo) * static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }

private:
    uint64_t state_;
};

}  // namespace kindling
