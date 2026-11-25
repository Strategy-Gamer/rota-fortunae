#include "countries/control.h"

#include "core/world.h"
#include "utility/FixedMath.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace rota::countries {

namespace {

using rota::map::LocationID;
using rota::map::LocationKind;
using rota::map::Terrain;

// Effective metres per km travelled: how much slower than open ground
const std::int64_t SEA_COST = 1000;
const std::int64_t TERRAIN_COST[static_cast<std::size_t>(Terrain::Count)] = {
    1000, // None
    1000, 1000, 1000, 1000, 1000, 1000, // grasslands, farmlands, steppe, savannah, drylands, coastline
    1250, 1500,                         // coastal desert, desert
    1250, 1750, 3000, 2500,             // woods, forest, jungle, marsh
    1500, 1750, 3000, 4000,             // hills, highlands, mountain, glacier
};

const std::int64_t CONTROL_HALF_DISTANCE = 1'750'000; // effective metres over which control halves
const std::int64_t CONTROL_CURVE_HALVINGS = 12;       // past this, control stays at its last sample

const std::int64_t UNREACHED = std::numeric_limits<std::int64_t>::max();

std::int64_t travel_cost(const rota::map::Geography& g, LocationID loc) {
    return g.kind[loc] == LocationKind::Sea ? SEA_COST : TERRAIN_COST[static_cast<std::size_t>(g.terrain[loc])];
}

// 2^-halvings, sampled once
const rota::fmath::SampledCurve& halving_curve() {
    static const rota::fmath::SampledCurve curve(Fixed64(CONTROL_CURVE_HALVINGS), CONTROL_CURVE_HALVINGS * 100,
        [](Fixed64 halvings) { return rota::fmath::pow(Fixed64::from_raw(Fixed64::SCALE / 2), halvings); });
    return curve;
}

}

std::vector<Fixed64> compute_control(const rota::core::World& w) {
    const auto& g = w.geography;
    const auto& owner = w.location_politics.owner_country;
    const LocationID location_count = g.location_count();

    const auto& halving = halving_curve();
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
            if (owner[loc] == c) control[loc] = halving(Fixed64(d) / CONTROL_HALF_DISTANCE);

            for (std::uint32_t i = g.neighbor_offset[loc]; i < g.neighbor_offset[loc + 1]; i++) {
                const LocationID next = g.neighbors[i];
                const bool sea = g.kind[next] == LocationKind::Sea;
                if (!sea && owner[next] != c) continue; // the owner's own land and the sea; never foreign or impassable land

                // half the edge lies in each location
                const std::int64_t next_distance = d + g.neighbor_distance[i] * (travel_cost(g, loc) + travel_cost(g, next)) / 2;
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
