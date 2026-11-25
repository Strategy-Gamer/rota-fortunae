#include "world.h"

#include "hash.h"

namespace rota::core {

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