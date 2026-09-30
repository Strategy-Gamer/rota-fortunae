#pragma once

#include <cstdint>

#include "../utility/FixedDecimal.h"
#include "../map/geography.h"

namespace rota::core { struct World; }

namespace rota::queries {
struct LocationSummary {
    std::uint64_t   population;
    Fixed64         wealth;
    Fixed32         productivity;
    Fixed64         wealth_per_capita;  // Derived
    std::int32_t    owner_country;      // -1 = unowned  
};
LocationSummary build_location_summary(const rota::core::World& world, rota::map::LocationID loc);
}