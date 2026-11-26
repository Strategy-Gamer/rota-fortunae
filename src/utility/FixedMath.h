#pragma once

#include "FixedDecimal.h"

#include <cstdint>
#include <vector>

// Deterministic fixed-point transcendentals for Fixed64 (6-decimal). Integer-only math,
// so results are bit-identical across platforms (needed for the lockstep checksum).
// Accuracy target ~1e-4 over the model's domain; the display gauges reuse these too.
// Domain assumptions: sqrt(x>=0); ln(x>0); pow(base>0); exp(x) roughly in [-40, 40].

namespace rota::fmath {

Fixed64 sqrt(Fixed64 x);
Fixed64 exp(Fixed64 x);
Fixed64 ln(Fixed64 x);
Fixed64 pow(Fixed64 base, Fixed64 exponent);
Fixed64 logistic(Fixed64 x, Fixed64 k, Fixed64 x0);

// floor(sqrt(n)) of a plain integer
std::uint64_t isqrt(std::uint64_t n);

// A curve sampled at even steps over [0, max] and read back by integer linear interpolation, for hot
// paths where a series per call costs too much. Below 0 it reads the first sample, past max the last.
// Build it on first use (a function-local static), never during static init: these functions read
// constants that another translation unit initializes.
class SampledCurve {
public:
    template <typename F>
    SampledCurve(Fixed64 max, std::int64_t intervals, F curve) : step(max.raw_value() / intervals) {
        samples.reserve(static_cast<std::size_t>(intervals) + 1);
        for (std::int64_t i = 0; i <= intervals; i++) samples.push_back(curve(Fixed64::from_raw(i * step)));
    }

    Fixed64 operator()(Fixed64 x) const {
        const std::int64_t raw = x.raw_value();
        if (raw <= 0) return samples.front();
        const std::size_t i = static_cast<std::size_t>(raw / step);
        if (i + 1 >= samples.size()) return samples.back();
        const std::int64_t a = samples[i].raw_value();
        const std::int64_t b = samples[i + 1].raw_value();
        return Fixed64::from_raw(a + (b - a) * (raw - static_cast<std::int64_t>(i) * step) / step);
    }

private:
    std::int64_t step;
    std::vector<Fixed64> samples;
};

}
