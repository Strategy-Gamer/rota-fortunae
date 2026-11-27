#include "location_summary.h"

#include "../core/world.h"
#include "../countries/control.h"
#include "../economy/secular_cycle_system.h"

namespace rota::queries {

LocationSummary build_location_summary(const rota::core::World& world, rota::map::LocationID loc){
    const auto& eco = world.location_economy;
    const auto& cyc = world.location_secular_cycle;
    const rota::countries::CountryID owner = world.location_politics.owner_country[loc];

    const std::vector<Fixed64> control = rota::countries::compute_control(world);
    Fixed64 reached_elites;
    if (world.countries.is_valid(owner)) {
        for (rota::map::LocationID other = 0; other < world.geography.location_count(); other++) {
            if (world.location_politics.owner_country[other] == owner) reached_elites += control[other] * static_cast<std::int64_t>(eco.elites[other]);
        }
    }

    LocationSummary s;
    s.commoners          = eco.commoners[loc];
    s.elites             = eco.elites[loc];
    s.population         = s.commoners + s.elites;
    s.wealth             = eco.commoner_wealth[loc] + eco.elite_wealth[loc];
    s.wealth_per_capita  = s.population > 0
        ? s.wealth / static_cast<int64_t>(s.population)
        : Fixed64();
    s.population_density = Fixed64(static_cast<std::int64_t>(s.commoners)) / rota::economy::LOCATION_CAPACITY;
    s.elite_positions    = rota::economy::elite_positions(world, loc, control[loc], reached_elites);
    s.elite_overproduction = rota::economy::elite_overproduction_gauge(
        static_cast<std::int64_t>(s.elites) / s.elite_positions);
    s.commoner_satisfaction = cyc.commoner_satisfaction[loc];
    s.elite_satisfaction = cyc.elite_satisfaction[loc];
    s.effective_radicalism = cyc.effective_radicalism[loc];
    s.security           = cyc.security[loc];
    s.control            = control[loc];
    s.owner_country      = world.countries.is_valid(owner) ? static_cast<std::int32_t>(owner) : -1;
    s.controller         = world.countries.is_valid(owner) ? static_cast<std::int32_t>(world.location_politics.controller(loc)) : -1;
    return s;
}

}
