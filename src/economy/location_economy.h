#pragma once

#include "../map/geography.h"
#include "../utility/FixedDecimal.h"

#include <cstdint>
#include <vector>

namespace rota::core { class Hasher; }

namespace rota::economy {

struct LocationEconomy {
    // Indexed by LocationID.

    void initialize(std::uint32_t location_count);

    std::vector<std::uint32_t> population;
    std::vector<Fixed32> productivity;
    std::vector<Fixed64> wealth;
    
    void clear();
    void feed_hash(rota::core::Hasher& h) const;
};
}