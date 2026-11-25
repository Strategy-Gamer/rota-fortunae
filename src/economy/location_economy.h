#pragma once

#include "../map/geography.h"
#include "../utility/FixedDecimal.h"

#include <cstdint>
#include <vector>

namespace rota::core { class Hasher; }

namespace rota::economy {

// People that one location's land supports, until terrain gives locations their own
inline constexpr std::int64_t LOCATION_CAPACITY = 100'000;

struct LocationEconomy {
    // Indexed by LocationID.

    void initialize(std::uint32_t location_count);

    std::vector<std::uint32_t> commoners;
    std::vector<std::uint32_t> elites;
    std::vector<Fixed64> commoner_wealth; // this year's income, not an accumulating stock
    std::vector<Fixed64> elite_wealth;

    void clear();
    void feed_hash(rota::core::Hasher& h) const;
};
}
