#pragma once

#include <cstdint>

#include "../utility/FixedDecimal.h"
#include "../map/geography.h"

namespace rota::core { struct World; }

namespace rota::queries {
struct LocationSummary {
    std::uint64_t   population;         // commoners + elites
    std::uint64_t   elites;
    Fixed64         wealth;
    Fixed64         wealth_per_capita;  // Derived
    Fixed64         unrest;
    Fixed64         effective_unrest;
    Fixed64         security;
    std::int32_t    owner_country;      // -1 = unowned
};
LocationSummary build_location_summary(const rota::core::World& world, rota::map::LocationID loc);
}
