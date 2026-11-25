#pragma once

#include "../utility/FixedDecimal.h"
#include "../utility/FixedMath.h"

#include <algorithm>
#include <cstdint>

// Fixed-point translations of Planning/extra_functions.py, taking headcounts as integers so that
// products of two headcounts stay in int64 instead of Fixed64's 128-bit long-division path.

namespace rota::economy {

// Share of a land's wealth that goes to its workers; falls as workers crowd the jobs (pressure =
// workers / jobs). worker_owned_count is always 0 now (all land is elite-owned), so it is left out.
inline Fixed64 get_wage_share(Fixed64 pressure, Fixed64 k, Fixed64 d_crit) {
    const Fixed64 base = rota::fmath::logistic(pressure, k, d_crit);
    const Fixed64 share = base * Fixed64::from_raw(750'000) + Fixed64::from_raw(100'000);
    return std::clamp(share, Fixed64::from_raw(100'000), Fixed64::from_raw(900'000));
}

// get_output per unit of base productivity, at saturation 1 and exponent 1:
// workers * (jpw / (jpw + 1)) reduces to jobs * workers / (jobs + workers)
inline Fixed64 get_output(std::int64_t jobs, std::int64_t workers) {
    if (jobs <= 0 || workers <= 0) return Fixed64();
    return Fixed64::from_raw(jobs * workers * Fixed64::SCALE / (jobs + workers));
}

}
