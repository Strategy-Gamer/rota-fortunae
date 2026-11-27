#pragma once

#include "../map/geography.h"
#include "../utility/FixedDecimal.h"

#include <cstdint>
#include <vector>

namespace rota::core { class Hasher; }

namespace rota::economy {

// People a location's land supports until something gives it its own
inline constexpr std::uint32_t DEFAULT_CAPACITY = 100'000;

struct LocationEconomy {
    // Indexed by LocationID.

    void initialize(std::uint32_t location_count);

    std::vector<std::uint32_t> capacity; // people its land supports, as jobs and as the population's limit
    std::vector<std::uint32_t> commoners;
    std::vector<std::uint32_t> elites;
    std::vector<Fixed64> commoner_wealth; // this year's income, not an accumulating stock
    std::vector<Fixed64> elite_wealth;

    void clear();
    void feed_hash(rota::core::Hasher& h) const;
};
}
