#pragma once

#include "../utility/FixedDecimal.h"
#include "../utility/FixedMath.h"

#include <algorithm>

// Fixed-point translations of Planning/extra_functions.py

namespace rota::economy {

// Share of a land's wealth that goes to its workers; falls as workers crowd the jobs.
// worker_owned_count is always 0 now (all land is elite-owned), so it is left out.
inline Fixed64 get_wage_share(Fixed64 workers, Fixed64 jobs, Fixed64 k, Fixed64 d_crit) {
    if (jobs <= Fixed64()) return Fixed64();
    const Fixed64 base = rota::fmath::logistic(workers / jobs, k, d_crit);
    const Fixed64 share = base * Fixed64::from_raw(750'000) + Fixed64::from_raw(100'000);
    return std::clamp(share, Fixed64::from_raw(100'000), Fixed64::from_raw(900'000));
}

// get_output at saturation 1, exponent 1: workers * base * (jpw / (jpw + 1)) reduces to this
inline Fixed64 get_output(Fixed64 jobs, Fixed64 workers, Fixed64 base_prod) {
    if (jobs <= Fixed64() || workers <= Fixed64()) return Fixed64();
    return base_prod * (jobs * workers / (jobs + workers));
}

}
