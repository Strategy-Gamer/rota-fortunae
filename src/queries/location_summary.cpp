#include "location_summary.h"

#include "../core/world.h"
#include "../economy/secular_cycle_system.h"

namespace rota::queries {

LocationSummary build_location_summary(const rota::core::World& world, rota::map::LocationID loc){
    const auto& eco = world.location_economy;
    const auto& cyc = world.location_secular_cycle;
    const rota::countries::CountryID owner = world.location_politics.owner_country[loc];

    std::int64_t country_elites = 0;
    if (world.countries.is_valid(owner)) {
        for (rota::map::LocationID other = 0; other < world.geography.location_count(); other++) {
            if (world.location_politics.owner_country[other] == owner) country_elites += eco.elites[other];
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
    s.elite_positions    = rota::economy::elite_positions(world, loc, country_elites);
    s.elite_overproduction = rota::economy::elite_overproduction_gauge(
        static_cast<std::int64_t>(s.elites) / s.elite_positions);
    s.unrest             = cyc.unrest[loc];
    s.effective_unrest   = cyc.effective_unrest[loc];
    s.security           = cyc.security[loc];
    s.owner_country      = world.countries.is_valid(owner) ? static_cast<std::int32_t>(owner) : -1;
    return s;
}

}
