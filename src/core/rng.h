#pragma once

#include <cstdint>

namespace rota::core {

// SplitMix64: integer-only, so every platform draws the same sequence from the same seed
struct Rng {
    std::uint64_t state;

    std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    // Uniform in [0, n)
    std::uint32_t below(std::uint32_t n) {
        return static_cast<std::uint32_t>(((next() >> 32) * n) >> 32);
    }
};

// A draw that depends only on its key, so it needs no stored stream and no fixed call order
inline std::uint64_t keyed_hash(std::uint64_t seed, std::uint64_t a, std::uint64_t b, std::uint64_t c) {
    Rng r{ seed };
    r.state ^= Rng{ a }.next();
    r.state ^= Rng{ b ^ 0xA0761D6478BD642FULL }.next();
    r.state ^= Rng{ c ^ 0xE7037ED1A0B428DBULL }.next();
    return r.next();
}

}
