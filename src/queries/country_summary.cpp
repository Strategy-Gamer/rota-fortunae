#include "country_summary.h"

#include "../core/world.h"
#include "../economy/secular_cycle_system.h"
#include "../utility/FixedMath.h"

#include <algorithm>

namespace rota::queries {

namespace {
const Fixed64 P_STEEPNESS(3);  // gentle, so the gauge tracks real population in a Turchin-like band
const Fixed64 E_SPAN(2);       // surplus ratio that reads as E = 1
}

CountrySummary build_country_summary(const rota::core::World& world, rota::countries::CountryID country){
    CountrySummary s{};
    s.location_count = world.countries.location_count[country];

    const auto& eco = world.location_economy;
    const auto& cyc = world.location_secular_cycle;
    const auto& state = world.country_state;

    std::uint64_t commoners = 0;
    Fixed64 positions, unrest, effective_unrest;
    const std::uint32_t location_count = world.geography.location_count();
    for (rota::map::LocationID loc = 0; loc < location_count; loc++) {
        if (world.location_politics.owner_country[loc] != country) continue;
        const Fixed64 weight(static_cast<std::int64_t>(eco.commoners[loc]));
        commoners        += eco.commoners[loc];
        s.elites         += eco.elites[loc];
        s.wealth         += eco.commoner_wealth[loc] + eco.elite_wealth[loc];
        positions        += rota::economy::local_elite_positions(world, loc);
        unrest           += cyc.unrest[loc] * weight;
        effective_unrest += cyc.effective_unrest[loc] * weight;
    }
    s.population = commoners + s.elites;

    s.wealth_per_capita = s.population > 0
        ? s.wealth / static_cast<int64_t>(s.population)
        : Fixed64();

    s.phase         = state.phase[country];
    s.legitimacy    = state.legitimacy[country];
    s.treasury      = state.treasury[country];
    s.war_weariness = state.war_weariness[country];
    for (std::uint32_t t = 0; t < rota::countries::CRISIS_COUNT; t++) {
        if (state.crises[country][t].active) s.active_crises |= 1U << t;
    }

    if (s.location_count == 0 || commoners == 0) return s;

    const Fixed64 total_commoners(static_cast<std::int64_t>(commoners));
    s.unrest           = unrest / total_commoners;
    s.effective_unrest = effective_unrest / total_commoners;

    const Fixed64 capacity(rota::economy::LOCATION_CAPACITY * static_cast<std::int64_t>(s.location_count));
    s.population_pressure = rota::fmath::logistic(total_commoners / capacity, P_STEEPNESS, Fixed64(1));

    const Fixed64 overproduction = Fixed64(static_cast<std::int64_t>(s.elites)) / (positions + state.state_positions[country]);
    s.elite_overproduction = std::clamp((overproduction - Fixed64(1)) / E_SPAN, Fixed64(), Fixed64(1));
    return s;
}

}
