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
    w.seed = seed;
    rota::core::Rng rng{ seed };
    const std::uint32_t location_count = w.geography.location_count();

    std::vector<rota::map::LocationID> capitals;
    while (capitals.size() < country_count) {
        const rota::map::LocationID loc = rng.below(location_count);
        if (w.geography.is_land(loc) && std::find(capitals.begin(), capitals.end(), loc) == capitals.end()) {
            capitals.push_back(loc);
        }
    }

    // Each country starts at its own point of the growth phase, so cycles don't march in step
    std::vector<rota::countries::CountryID> countries;
    std::vector<std::uint32_t> start_commoners;
    for (std::uint32_t i = 0; i < country_count; i++) {
        const std::uint32_t color = static_cast<std::uint32_t>(rng.next()) & 0xFFFFFFU;
        countries.push_back(rota::countries::create_country(
            w, "Country " + std::to_string(i + 1), color, w.calendar.get_year()));
        const std::int64_t per_million = 400'000 + rng.below(500'001);
        start_commoners.push_back(static_cast<std::uint32_t>(
            rota::economy::LOCATION_CAPACITY * per_million / 1'000'000));
    }

    // Every country grows from its capital over land one ring at a time, so territories are
    // contiguous; land no capital can reach by land or strait stays unowned
    constexpr std::uint32_t UNCLAIMED = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> claim(location_count, UNCLAIMED);
    std::vector<rota::map::LocationID> queue;
    for (std::uint32_t i = 0; i < country_count; i++) {
        claim[capitals[i]] = i;
        queue.push_back(capitals[i]);
    }
    for (std::size_t head = 0; head < queue.size(); head++) {
        const rota::map::LocationID loc = queue[head];
        const auto& g = w.geography;
        for (std::uint32_t n = g.neighbor_offset[loc]; n < g.neighbor_offset[loc + 1]; n++) {
            const rota::map::LocationID next = g.neighbors[n];
            if (g.is_land(next) && claim[next] == UNCLAIMED) {
                claim[next] = claim[loc];
                queue.push_back(next);
            }
        }
    }

    for (const rota::map::LocationID loc : queue) {
        rota::countries::set_location_owner(w, loc, countries[claim[loc]]);
        w.location_economy.commoners[loc] = start_commoners[claim[loc]];
    }
}

}
