#include "location_summary.h"

#include "../core/world.h"

namespace rota::queries {

LocationSummary build_location_summary(const rota::core::World& world, rota::map::LocationID loc){
    LocationSummary s;
    s.population        = world.location_economy.population[loc];
    s.wealth            = world.location_economy.wealth[loc];
    s.productivity      = world.location_economy.productivity[loc];
    s.wealth_per_capita = s.population > 0
        ? s.wealth / static_cast<int64_t>(s.population)
        : Fixed64();
    s.owner_country     = world.location_politics.owner_country[loc];
    return s;
}

}