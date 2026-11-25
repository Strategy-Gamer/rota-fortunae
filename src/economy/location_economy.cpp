#include "location_economy.h"

#include "../core/hash.h"

namespace rota::economy {
    void LocationEconomy::initialize(std::uint32_t count) {
        commoners.assign(count, static_cast<std::uint32_t>(LOCATION_CAPACITY * 6 / 10)); // FinalSim's start
        elites.assign(count, static_cast<std::uint32_t>(LOCATION_CAPACITY * 2 / 100));
        commoner_wealth.assign(count, Fixed64());
        elite_wealth.assign(count, Fixed64());
    }

    void LocationEconomy::clear(){
        commoners.clear();
        elites.clear();
        commoner_wealth.clear();
        elite_wealth.clear();
    }

    void LocationEconomy::feed_hash(rota::core::Hasher& h) const{
        size_t location_count = commoners.size();
        for (size_t i = 0; i < location_count; i++) h.feed(commoners[i]);
        for (size_t i = 0; i < location_count; i++) h.feed(elites[i]);
        for (size_t i = 0; i < location_count; i++) h.feed(commoner_wealth[i].raw_value());
        for (size_t i = 0; i < location_count; i++) h.feed(elite_wealth[i].raw_value());
    }
}
