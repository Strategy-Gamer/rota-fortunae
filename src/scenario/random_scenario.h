#pragma once

#include <cstdint>

namespace rota::core { struct World; }

namespace rota::scenario {

// Places country_count countries on random capitals; each grows outward one neighbor at a time over
// land and straits, and land no capital reaches stays unowned. Expects a freshly built map with no countries.
void generate_random_scenario(
    rota::core::World& w,
    std::uint64_t seed,
    std::uint32_t country_count
);

}
