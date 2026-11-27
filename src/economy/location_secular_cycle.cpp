#include "location_secular_cycle.h"

#include "../core/hash.h"

namespace rota::economy {

void LocationSecularCycle::initialize(std::uint32_t count) {
    commoner_satisfaction.assign(count, Fixed64(1));
    elite_satisfaction.assign(count, Fixed64(1));
    effective_radicalism.assign(count, Fixed64());
    security.assign(count, Fixed64(1));
    fertility.assign(count, Fixed64(1));
}

void LocationSecularCycle::clear() {
    commoner_satisfaction.clear();
    elite_satisfaction.clear();
    effective_radicalism.clear();
    security.clear();
    fertility.clear();
}

void LocationSecularCycle::feed_hash(rota::core::Hasher& h) const {
    const size_t location_count = security.size();
    for (size_t i = 0; i < location_count; i++) h.feed(commoner_satisfaction[i].raw_value());
    for (size_t i = 0; i < location_count; i++) h.feed(elite_satisfaction[i].raw_value());
    for (size_t i = 0; i < location_count; i++) h.feed(effective_radicalism[i].raw_value());
    for (size_t i = 0; i < location_count; i++) h.feed(security[i].raw_value());
    for (size_t i = 0; i < location_count; i++) h.feed(fertility[i].raw_value());
}

}
