#pragma once

#include <cstdint>

#include "../utility/FixedDecimal.h"
#include "../countries/countries.h"

namespace rota::core { struct World; }

namespace rota::queries {
struct CountrySummary {
    std::uint32_t   location_count;
    std::uint64_t   population;         // commoners + elites
    std::uint64_t   elites;
    Fixed64         wealth;
    Fixed64         wealth_per_capita;  // Derived

    // Secular-cycle read-outs; FinalSim's four gauges are P, E, U and S (= legitimacy)
    std::uint8_t    phase;
    Fixed64         population_pressure;
    Fixed64         elite_overproduction;
    Fixed64         unrest;             // pop-weighted mobilization stock
    Fixed64         effective_unrest;   // pop-weighted, after suppression
    Fixed64         legitimacy;
    Fixed64         treasury;
    Fixed64         war_weariness;
    std::uint32_t   active_crises;      // bit per CrisisType
};
CountrySummary build_country_summary(const rota::core::World& world, rota::countries::CountryID country);
}
