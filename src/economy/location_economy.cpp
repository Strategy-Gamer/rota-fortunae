#include "location_economy.h"

#include "../core/hash.h"

namespace rota::economy {
    void LocationEconomy::initialize(std::uint32_t count) {
        capacity.assign(count, DEFAULT_CAPACITY);
        commoners.assign(count, DEFAULT_CAPACITY * 6 / 10); // FinalSim's start
        elites.assign(count, DEFAULT_CAPACITY * 2 / 100);
        commoner_wealth.assign(count, Fixed64());
        elite_wealth.assign(count, Fixed64());
    }

    void LocationEconomy::clear(){
        capacity.clear();
        commoners.clear();
        elites.clear();
        commoner_wealth.clear();
        elite_wealth.clear();
    }

    void LocationEconomy::feed_hash(rota::core::Hasher& h) const{
        size_t location_count = commoners.size();
        for (size_t i = 0; i < location_count; i++) h.feed(capacity[i]);
        for (size_t i = 0; i < location_count; i++) h.feed(commoners[i]);
        for (size_t i = 0; i < location_count; i++) h.feed(elites[i]);
        for (size_t i = 0; i < location_count; i++) h.feed(commoner_wealth[i].raw_value());
        for (size_t i = 0; i < location_count; i++) h.feed(elite_wealth[i].raw_value());
    }
}
