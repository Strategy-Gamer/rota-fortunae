#include "countries/control.h"

#include "core/world.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace rota::countries {

namespace {

using rota::map::LocationID;
using rota::map::LocationKind;

const std::int64_t SEA_COST = 2;                // per pixel of an edge touching sea; land costs 1
const std::int64_t CONTROL_HALF_DISTANCE = 150; // effective distance at which control halves

const std::int64_t UNREACHED = std::numeric_limits<std::int64_t>::max();

}

std::vector<Fixed64> compute_control(const rota::core::World& w) {
    const auto& g = w.geography;
    const auto& owner = w.location_politics.owner_country;
    const LocationID location_count = g.location_count();

    std::vector<Fixed64> control(location_count);
    std::vector<std::int64_t> distance(location_count, UNREACHED);
    std::vector<LocationID> reached;
    using Entry = std::pair<std::int64_t, LocationID>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;

    const CountryID slot_count = static_cast<CountryID>(w.countries.alive.size());
    for (CountryID c = 0; c < slot_count; c++) {
        if (!w.countries.is_valid(c)) continue;
        const LocationID capital = w.countries.capital[c];
        if (capital == rota::map::INVALID_LOCATION_ID) continue;

        // Shortest paths are unique in length, so the result doesn't depend on the queue's tie order
        distance[capital] = 0;
        reached.push_back(capital);
        open.emplace(0, capital);
        while (!open.empty()) {
            const auto [d, loc] = open.top();
            open.pop();
            if (d > distance[loc]) continue;
            if (owner[loc] == c) control[loc] = Fixed64(CONTROL_HALF_DISTANCE) / (CONTROL_HALF_DISTANCE + d);

            for (std::uint32_t i = g.neighbor_offset[loc]; i < g.neighbor_offset[loc + 1]; i++) {
                const LocationID next = g.neighbors[i];
                const bool sea = g.kind[next] == LocationKind::Sea;
                if (!sea && owner[next] != c) continue; // the owner's own land and the sea; never foreign or impassable land

                const bool sea_edge = sea || g.kind[loc] == LocationKind::Sea;
                const std::int64_t next_distance = d + g.neighbor_distance[i] * (sea_edge ? SEA_COST : 1);
                if (next_distance < distance[next]) {
                    if (distance[next] == UNREACHED) reached.push_back(next);
                    distance[next] = next_distance;
                    open.emplace(next_distance, next);
                }
            }
        }

        for (const LocationID loc : reached) distance[loc] = UNREACHED;
        reached.clear();
    }

    return control;
}

}
