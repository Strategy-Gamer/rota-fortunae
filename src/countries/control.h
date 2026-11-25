#pragma once

#include "utility/FixedDecimal.h"

#include <vector>

namespace rota::core { struct World; }

namespace rota::countries {

// Each location's control by its owner, indexed by LocationID: 1 at the capital, falling with
// effective distance from it over land the owner holds and the sea. Zero for unowned land,
// occupied land, land the owner can't reach, and countries without a capital. Derived from
// ownership, occupation, capitals and the map, so it's recomputed rather than stored.
std::vector<Fixed64> compute_control(const rota::core::World& w);

}
