// SHA-256 (FIPS 180-4), for checking a save's bytes before any of them are
// trusted.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace kindling {

class Sha256 {
public:
    Sha256() { reset(); }
    void reset();
    void update(const void* data, size_t n);
    std::array<uint8_t, 32> finish();  // resets afterwards

private:
    void block(const uint8_t* p);
    uint32_t h_[8];
    uint8_t buf_[64];
    size_t used_ = 0;
    uint64_t total_ = 0;
};

std::array<uint8_t, 32> sha256(const void* data, size_t n);
std::string toHex(const std::array<uint8_t, 32>& digest);

}  // namespace kindling
