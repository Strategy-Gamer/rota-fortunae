#include "scenario/random_scenario.h"

#include "core/rng.h"
#include "core/world.h"
#include "countries/country_systems.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace rota::scenario {

void generate_random_scenario(
    rota::core::World& w,
    std::uint64_t seed,
    std::uint32_t country_count
) {
    rota::core::Rng rng{ seed };
    const std::uint32_t location_count = w.geography.location_count();

    std::vector<rota::map::LocationID> capitals;
    while (capitals.size() < country_count) {
        const rota::map::LocationID loc = rng.below(location_count);
        if (std::find(capitals.begin(), capitals.end(), loc) == capitals.end()) {
            capitals.push_back(loc);
        }
    }

    std::vector<rota::countries::CountryID> countries;
    for (std::uint32_t i = 0; i < country_count; i++) {
        const std::uint32_t color = static_cast<std::uint32_t>(rng.next()) & 0xFFFFFFU;
        countries.push_back(rota::countries::create_country(
            w, "Country " + std::to_string(i + 1), color, w.calendar.get_year()));
    }

    for (rota::map::LocationID loc = 0; loc < location_count; loc++) {
        std::uint32_t nearest = 0;
        std::int64_t nearest_dist = std::numeric_limits<std::int64_t>::max();

        for (std::uint32_t i = 0; i < country_count; i++) {
            const std::int64_t dx = w.geography.centroid_x[loc] - w.geography.centroid_x[capitals[i]];
            const std::int64_t dy = w.geography.centroid_y[loc] - w.geography.centroid_y[capitals[i]];
            const std::int64_t dist = dx * dx + dy * dy;
            if (dist < nearest_dist) {
                nearest = i;
                nearest_dist = dist;
            }
        }

        rota::countries::set_location_owner(w, loc, countries[nearest]);
    }
}

}
