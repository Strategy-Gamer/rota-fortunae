#include "location_economy.h"

#include "../core/hash.h"

namespace rota::economy {
    void LocationEconomy::initialize(std::uint32_t count) {
        population.assign(count, 1000);                     // placeholder seed so the panel
        productivity.assign(count, Fixed32::from_raw(1000)); // shows something (=1.000);
        wealth.assign(count, Fixed64());                     // real values come from scenario setup (W2)
    }

    void LocationEconomy::clear(){
        population.clear();
        productivity.clear();
        wealth.clear();
    }

    void LocationEconomy::feed_hash(rota::core::Hasher& h) const{
        size_t location_count = population.size();
        for (size_t i = 0; i < location_count; i++) h.feed(population[i]);
        for (size_t i = 0; i < location_count; i++) h.feed(productivity[i].raw_value());
        for (size_t i = 0; i < location_count; i++) h.feed(wealth[i].raw_value());
    }
}