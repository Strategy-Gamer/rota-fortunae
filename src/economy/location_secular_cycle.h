#pragma once

#include "../utility/FixedDecimal.h"

#include <cstdint>
#include <vector>

namespace rota::core { class Hasher; }

namespace rota::economy {

struct LocationSecularCycle {
    // Indexed by LocationID.

    void initialize(std::uint32_t location_count);

    std::vector<Fixed64> unrest;           // FinalSim's U: the mobilization stock
    std::vector<Fixed64> effective_unrest; // U_e: unrest left after suppression
    std::vector<Fixed64> security;

    void clear();
    void feed_hash(rota::core::Hasher& h) const;
};

}
