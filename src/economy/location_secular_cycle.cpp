#include "location_secular_cycle.h"

#include "../core/hash.h"

namespace rota::economy {

void LocationSecularCycle::initialize(std::uint32_t count) {
    unrest.assign(count, Fixed64());
    effective_unrest.assign(count, Fixed64());
    security.assign(count, Fixed64(1));
}

void LocationSecularCycle::clear() {
    unrest.clear();
    effective_unrest.clear();
    security.clear();
}

void LocationSecularCycle::feed_hash(rota::core::Hasher& h) const {
    const size_t location_count = unrest.size();
    for (size_t i = 0; i < location_count; i++) h.feed(unrest[i].raw_value());
    for (size_t i = 0; i < location_count; i++) h.feed(effective_unrest[i].raw_value());
    for (size_t i = 0; i < location_count; i++) h.feed(security[i].raw_value());
}

}
