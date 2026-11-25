#include "country_summary.h"

#include "../core/world.h"

namespace rota::queries {

CountrySummary build_country_summary(const rota::core::World& world, rota::countries::CountryID country){
    CountrySummary s{};
    s.location_count = world.countries.location_count[country];

    const std::uint32_t location_count = world.geography.location_count();
    for (rota::map::LocationID loc = 0; loc < location_count; loc++) {
        if (world.location_politics.owner_country[loc] != country) continue;
        s.population += world.location_economy.population[loc];
        s.wealth     += world.location_economy.wealth[loc];
    }

    s.wealth_per_capita = s.population > 0
        ? s.wealth / static_cast<int64_t>(s.population)
        : Fixed64();
    return s;
}

}
