#pragma once

#include <cstdint>
#include <vector>

#include "../utility/FixedDecimal.h"
#include "../countries/countries.h"

namespace rota::core { struct World; }

namespace rota::queries {
struct CountrySummary {
    std::uint32_t   location_count;
    std::uint32_t   occupied;           // owned locations someone else holds
    std::uint32_t   generation;         // with the ID, the country's permanent key
    std::int32_t    rebels_against;     // -1 unless this is a rebel side
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

// Every country's summary from one pass over the locations, indexed by CountryID (dead slots zeroed)
std::vector<CountrySummary> build_country_summaries(const rota::core::World& world);
}
