#pragma once

#include <cstdint>

#include "../utility/FixedDecimal.h"
#include "../countries/countries.h"

namespace rota::core { struct World; }

namespace rota::queries {
struct CountrySummary {
    std::uint32_t   location_count;
    std::uint64_t   population;
    Fixed64         wealth;
    Fixed64         wealth_per_capita;  // Derived
};
CountrySummary build_country_summary(const rota::core::World& world, rota::countries::CountryID country);
}
