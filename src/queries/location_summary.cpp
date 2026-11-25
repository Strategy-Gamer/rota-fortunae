#include "location_summary.h"

#include "../core/world.h"

namespace rota::queries {

LocationSummary build_location_summary(const rota::core::World& world, rota::map::LocationID loc){
    const auto& eco = world.location_economy;
    const auto& cyc = world.location_secular_cycle;

    LocationSummary s;
    s.elites            = eco.elites[loc];
    s.population        = static_cast<std::uint64_t>(eco.commoners[loc]) + s.elites;
    s.wealth            = eco.commoner_wealth[loc] + eco.elite_wealth[loc];
    s.wealth_per_capita = s.population > 0
        ? s.wealth / static_cast<int64_t>(s.population)
        : Fixed64();
    s.unrest            = cyc.unrest[loc];
    s.effective_unrest  = cyc.effective_unrest[loc];
    s.security          = cyc.security[loc];
    s.owner_country     = world.location_politics.owner_country[loc];
    return s;
}

}
