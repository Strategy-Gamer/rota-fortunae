#include "world.h"

#include "hash.h"
#include "../map/map_builder.h"

namespace rota::core {

void World::load_map(
    std::uint32_t width,
    std::uint32_t height,
    const std::uint8_t* rgb_bytes,
    std::size_t rgb_byte_count,
    const rota::map::LocationData& data
) {
    rota::map::MapBuilder::build_from_rgb8(geography, width, height, rgb_bytes, rgb_byte_count, data);
    const std::uint32_t location_count = geography.location_count();
    location_politics.initialize(location_count);
    location_economy.initialize(location_count);
    location_secular_cycle.initialize(location_count);
}

void World::clear() {
    seed = 0;
    geography.clear();
    countries.clear();
    country_state.clear();
    location_politics.clear();
    location_economy.clear();
    location_secular_cycle.clear();
    cycle_ledger.clear();
    calendar.clear();
    synch_clock.clear();
}

std::uint64_t state_hash(const World& world) {
    Hasher h;

    world.synch_clock.feed_hash(h);
    world.calendar.feed_hash(h);
    h.feed(world.seed);

    for (auto owner : world.location_politics.owner_country) h.feed(owner);
    for (auto occupier : world.location_politics.occupier) h.feed(occupier);

    world.countries.feed_hash(h);
    world.country_state.feed_hash(h);
    world.location_economy.feed_hash(h);
    world.location_secular_cycle.feed_hash(h);

    return h.value();
}

}