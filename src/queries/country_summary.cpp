#include "country_summary.h"

#include "../core/world.h"
#include "../economy/secular_cycle_system.h"

namespace rota::queries {

std::vector<CountrySummary> build_country_summaries(const rota::core::World& world){
    const auto& eco = world.location_economy;
    const auto& cyc = world.location_secular_cycle;
    const auto& state = world.country_state;

    struct Sums {
        std::uint64_t commoners = 0;
        Fixed64 positions, commoner_satisfaction, elite_satisfaction, effective_radicalism;
    };
    const std::size_t slot_count = world.countries.alive.size();
    std::vector<CountrySummary> out(slot_count, CountrySummary{});
    std::vector<Sums> sums(slot_count);

    const std::uint32_t location_count = world.geography.location_count();
    for (rota::map::LocationID loc = 0; loc < location_count; loc++) {
        const rota::countries::CountryID c = world.location_politics.owner_country[loc];
        if (!world.countries.is_valid(c)) continue;
        CountrySummary& s = out[c];
        Sums& t = sums[c];
        const std::int64_t commoners = eco.commoners[loc];
        t.commoners        += eco.commoners[loc];
        s.elites           += eco.elites[loc];
        s.capacity         += eco.capacity[loc];
        s.wealth           += eco.commoner_wealth[loc] + eco.elite_wealth[loc];
        t.positions        += rota::economy::local_elite_positions(world, loc);
        t.commoner_satisfaction += cyc.commoner_satisfaction[loc] * commoners;
        t.elite_satisfaction    += cyc.elite_satisfaction[loc] * static_cast<std::int64_t>(eco.elites[loc]);
        t.effective_radicalism  += cyc.effective_radicalism[loc] * commoners;
        if (world.location_politics.occupier[loc] != rota::countries::INVALID_COUNTRY_ID) s.occupied += 1;
    }

    for (rota::countries::CountryID c = 0; c < slot_count; c++) {
        if (!world.countries.is_valid(c)) continue;
        CountrySummary& s = out[c];
        const Sums& t = sums[c];

        s.location_count = world.countries.location_count[c];
        s.generation = world.countries.generation[c];
        s.rebels_against = world.countries.rebels_against[c] != rota::countries::INVALID_COUNTRY_ID
            ? static_cast<std::int32_t>(world.countries.rebels_against[c])
            : -1;
        s.population = t.commoners + s.elites;
        s.wealth_per_capita = s.population > 0
            ? s.wealth / static_cast<int64_t>(s.population)
            : Fixed64();

        s.phase         = state.phase[c];
        s.legitimacy    = state.legitimacy[c];
        s.treasury      = state.treasury[c];
        s.war_weariness = state.war_weariness[c];
        s.radicalism    = state.radicalism[c];
        for (std::uint32_t type = 0; type < rota::countries::CRISIS_COUNT; type++) {
            if (state.crises[c][type].active) s.active_crises |= 1U << type;
        }

        if (s.location_count == 0 || t.commoners == 0) continue;
        const std::int64_t commoners = static_cast<std::int64_t>(t.commoners);
        s.commoner_satisfaction = t.commoner_satisfaction / commoners;
        s.elite_satisfaction    = t.elite_satisfaction / static_cast<std::int64_t>(s.elites);
        s.effective_radicalism  = t.effective_radicalism / commoners;
        s.population_pressure = rota::economy::population_pressure_gauge(
            commoners, static_cast<std::int64_t>(s.capacity));
        s.elite_overproduction = rota::economy::elite_overproduction_gauge(
            static_cast<std::int64_t>(s.elites) / (t.positions + state.state_positions[c]));
    }
    return out;
}

}
