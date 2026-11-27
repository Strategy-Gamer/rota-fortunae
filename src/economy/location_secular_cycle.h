#pragma once

#include "../utility/FixedDecimal.h"

#include <cstdint>
#include <vector>

namespace rota::core { class Hasher; }

namespace rota::economy {

struct LocationSecularCycle {
    // Indexed by LocationID.

    void initialize(std::uint32_t location_count);

    std::vector<Fixed64> commoner_satisfaction; // with society at large, not with the government
    std::vector<Fixed64> elite_satisfaction;
    std::vector<Fixed64> effective_radicalism; // the owner's radicalism as it shows here, after suppression
    std::vector<Fixed64> security;
    std::vector<Fixed64> fertility; // share of the way from the lowest birth rate to the highest

    void clear();
    void feed_hash(rota::core::Hasher& h) const;
};

}
